/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <fstream>
#include "js/JSON.h"
#include "mozilla/JSONWriter.h"

#include "OpenVRSession.h"
#include "gfxPrefs.h"

#if defined(XP_WIN)
#include <d3d11.h>
#include "mozilla/gfx/DeviceManagerDx.h"
#elif defined(XP_MACOSX)
#include "mozilla/gfx/MacIOSurface.h"
#endif

#include "mozilla/dom/GamepadEventTypes.h"
#include "mozilla/dom/GamepadBinding.h"
#include "VRThread.h"

#if !defined(M_PI)
#define M_PI 3.14159265358979323846264338327950288
#endif

#define BTN_MASK_FROM_ID(_id) ::vr::ButtonMaskFromId(vr::EVRButtonId::_id)

// Haptic feedback is updated every 5ms, as this is
// the minimum period between new haptic pulse requests.
// Effectively, this results in a pulse width modulation
// with an interval of 5ms.  Through experimentation, the
// maximum duty cycle was found to be about 3.9ms
const uint32_t kVRHapticUpdateInterval = 5;

using namespace mozilla::gfx;

namespace mozilla {
namespace gfx {

namespace {

dom::GamepadHand
GetControllerHandFromControllerRole(::vr::ETrackedControllerRole aRole)
{
  dom::GamepadHand hand;

  switch (aRole) {
    case ::vr::ETrackedControllerRole::TrackedControllerRole_Invalid:
    case ::vr::ETrackedControllerRole::TrackedControllerRole_OptOut:
      hand = dom::GamepadHand::_empty;
      break;
    case ::vr::ETrackedControllerRole::TrackedControllerRole_LeftHand:
      hand = dom::GamepadHand::Left;
      break;
    case ::vr::ETrackedControllerRole::TrackedControllerRole_RightHand:
      hand = dom::GamepadHand::Right;
      break;
    default:
      hand = dom::GamepadHand::_empty;
      MOZ_ASSERT(false);
      break;
  }

  return hand;
}

void
UpdateButton(VRControllerState& aState,
             const ::vr::VRControllerState_t& aControllerState,
             uint32_t aButtonIndex,
             uint64_t aButtonMask)
{
  uint64_t mask = (1ULL << aButtonIndex);
  if ((aControllerState.ulButtonPressed & aButtonMask) == 0) {
    // not pressed
    aState.buttonPressed &= ~mask;
    aState.triggerValue[aButtonIndex] = 0.0f;
  } else {
    // pressed
    aState.buttonPressed |= mask;
    aState.triggerValue[aButtonIndex] = 1.0f;
  }
  if ((aControllerState.ulButtonTouched & aButtonMask) == 0) {
    // not touched
    aState.buttonTouched &= ~mask;
  } else {
    // touched
    aState.buttonTouched |= mask;
  }
}

}; // anonymous namespace

OpenVRSession::OpenVRSession()
  : VRSession()
  , mVRSystem(nullptr)
  , mVRChaperone(nullptr)
  , mVRCompositor(nullptr)
  , mControllerDeviceIndex{}
  , mHapticPulseRemaining{}
  , mHapticPulseIntensity{}
  , mIsWindowsMR(false)
  , mControllerHapticStateMutex("OpenVRSession::mControllerHapticStateMutex")
{
}

OpenVRSession::~OpenVRSession()
{
  Shutdown();
}

struct StringWriteFunc : public JSONWriteFunc
{
  nsAString& mBuffer; // This struct must not outlive this buffer
  explicit StringWriteFunc(nsAString& buffer)
    : mBuffer(buffer)
  {
  }

  void Write(const char* aStr) override
  {
    mBuffer.Append(NS_ConvertUTF8toUTF16(aStr));
  }
};

struct ControllerInfo_t
{
  vr::VRInputValueHandle_t m_source = vr::k_ulInvalidInputValueHandle;
  vr::VRActionHandle_t m_actionPose = vr::k_ulInvalidActionHandle;
  vr::VRActionHandle_t m_actionHaptic = vr::k_ulInvalidActionHandle;
  vr::VRActionHandle_t m_actionSkeletal = vr::k_ulInvalidActionHandle;
  vr::VRActionHandle_t m_actionTrackpad_Analog = vr::k_ulInvalidActionHandle;
  vr::VRActionHandle_t m_actionTrackpad_Pressed = vr::k_ulInvalidActionHandle;
  vr::VRActionHandle_t m_actionTrackpad_Touched = vr::k_ulInvalidActionHandle;

  vr::VRActionHandle_t m_actionTrigger_Value = vr::k_ulInvalidActionHandle;

  vr::VRActionHandle_t m_actionGrip_Pressed = vr::k_ulInvalidActionHandle;
  vr::VRActionHandle_t m_actionGrip_Touched = vr::k_ulInvalidActionHandle;

  vr::VRActionHandle_t m_actionMenu_Pressed = vr::k_ulInvalidActionHandle;
  vr::VRActionHandle_t m_actionMenu_Touched = vr::k_ulInvalidActionHandle;

  // gfx::Matrix4x4 m_rmat4Pose;
  //  CGLRenderModel *m_pRenderModel = nullptr;
  // std::string m_sRenderModelName;
  // bool m_bShowController;
};

enum EHand
{
  Left = 0,
  Right = 1,
  Total = 2
};

ControllerInfo_t m_rHand[2];
vr::VRActionSetHandle_t m_actionsetFirefox = vr::k_ulInvalidActionSetHandle;

bool
OpenVRSession::Initialize(mozilla::gfx::VRSystemState& aSystemState)
{
  if (!gfxPrefs::VREnabled() || !gfxPrefs::VROpenVREnabled()) {
    return false;
  }
  if (mVRSystem != nullptr) {
    // Already initialized
    return true;
  }
  if (!::vr::VR_IsRuntimeInstalled()) {
    return false;
  }
  if (!::vr::VR_IsHmdPresent()) {
    return false;
  }

  ::vr::HmdError err;

  ::vr::VR_Init(&err, ::vr::EVRApplicationType::VRApplication_Scene);
  if (err) {
    return false;
  }

  mVRSystem = (::vr::IVRSystem*)::vr::VR_GetGenericInterface(
    ::vr::IVRSystem_Version, &err);
  if (err || !mVRSystem) {
    Shutdown();
    return false;
  }
  mVRChaperone = (::vr::IVRChaperone*)::vr::VR_GetGenericInterface(
    ::vr::IVRChaperone_Version, &err);
  if (err || !mVRChaperone) {
    Shutdown();
    return false;
  }
  mVRCompositor = (::vr::IVRCompositor*)::vr::VR_GetGenericInterface(
    ::vr::IVRCompositor_Version, &err);
  if (err || !mVRCompositor) {
    Shutdown();
    return false;
  }

#if defined(XP_WIN)
  if (!CreateD3DObjects()) {
    Shutdown();
    return false;
  }

#endif

  // Configure coordinate system
  mVRCompositor->SetTrackingSpace(::vr::TrackingUniverseSeated);

  if (!InitState(aSystemState)) {
    Shutdown();
    return false;
  }

  if (gfxPrefs::VROpenVRActionInputEnabled()) {
    // Create and open a temp file
    std::string viveBindingFileName = std::tmpnam(nullptr);

    // TODO: Write it to profile folder.

    // In order to be more readable when editing this config file, we choose
    // don't use JSONWriter.
    const char* vive_controller =
      " \n"
      "{\n"
      "  \"controller_type\" : \"vive_controller\", \n"
      "  \"description\" : \"Bindings for Firefox OpenVR for the Vive controller\", \n"
      "  \"name\" : \"Firefox bindings for Vive Controller\", \n"
      "  \"bindings\" : { \n"
      "     \"/actions/firefox\" : { \n"
      "         \"poses\" : [        \n"
      "           {                \n"
      "             \"output\" : \"/actions/firefox/in/LHand_pose\", \n"
      "              \"path\" : \"/user/hand/left/pose/raw\"     \n"
      "            },\n"
      "            {                                             \n"
      "              \"output\" : \"/actions/firefox/in/RHand_pose\", \n"
      "              \"path\" : \"/user/hand/right/pose/raw\"    \n"
      "            }\n"
      "         ],\n"
      "         \"haptics\" : [ \n"
      "            {\n"
      "              \"output\" : \"/actions/firefox/out/LHand_haptic\",  \n"
      "              \"path\" : \"/user/hand/left/output/haptic\"     \n"
      "            },\n"
      "            { \n"
      "              \"output\" : \"/actions/firefox/out/RHand_haptic\", \n"
      "              \"path\" : \"/user/hand/right/output/haptic\"    \n"
      "            }\n"
      "		  ],\n"
      "       \"sources\" : [ \n"
      "           {\n"
      "             \"inputs\" : { \n"
      "                 \"click\" : { \n"
      "                    \"output\" : \"/actions/firefox/in/LHand_menu_pressed\"  \n"
      "                   }, \n"
      "                 \"touch\" : { \n"
      "                    \"output\" : \"/actions/firefox/in/LHand_menu_touched\"  \n"
      "                   } \n"
      "              },\n"
      "             \"mode\" : \"button\", \n"
      "             \"path\" : \"/user/hand/left/input/application_menu\" \n"
      "           }, \n"
      "           {\n"
      "             \"inputs\" : { \n"
      "                 \"click\" : { \n"
      "                    \"output\" : \"/actions/firefox/in/RHand_menu_pressed\"  \n"
      "                   }, \n"
      "                 \"touch\" : { \n"
      "                    \"output\" : \"/actions/firefox/in/RHand_menu_touched\"  \n"
      "                   } \n"
      "              },\n"
      "             \"mode\" : \"button\", \n"
      "             \"path\" : \"/user/hand/right/input/application_menu\" \n"
      "           }, \n"
      "           {\n"
      "             \"inputs\" : { \n"
      "                 \"pull\" : { \n"
      "                    \"output\" : \"/actions/firefox/in/LHand_trigger_value\"  \n"
      "                   } \n"
      "              },\n"
      "             \"mode\" : \"trigger\", \n"
      "             \"path\" : \"/user/hand/left/input/trigger\" \n"
      "           }, \n"
      "           {\n"
      "             \"inputs\" : { \n"
      "                 \"pull\" : { \n"
      "                    \"output\" : \"/actions/firefox/in/RHand_trigger_value\"  \n"
      "                   } \n"
      "              },\n"
      "             \"mode\" : \"trigger\", \n"
      "             \"path\" : \"/user/hand/right/input/trigger\" \n"
      "           }, \n"
      "           {\n"
      "             \"inputs\" : { \n"
      "               \"click\" : { \n"
      "                \"output\" : \"/actions/firefox/in/LHand_grip_pressed\" \n"
      "              }, \n"
      "               \"touch\" : { \n"
      "                 \"output\" : "
      "                  \"/actions/firefox/in/LHand_grip_touched\" \n"
      "              } \n"
      "             }, \n"
      "				      \"mode\" : \"button\", \n"
      "				      \"path\" : \"/user/hand/left/input/grip\" \n"
      "           }, \n"
      "           {\n"
      "             \"inputs\" : { \n"
      "               \"click\" : { \n"
      "                \"output\" : \"/actions/firefox/in/RHand_grip_pressed\" \n"
      "              }, \n"
      "               \"touch\" : { \n"
      "                 \"output\" : \"/actions/firefox/in/RHand_grip_touched\" \n"
      "              } \n"
      "           }, \n"
      "				      \"mode\" : \"button\", \n"
      "				      \"path\" : \"/user/hand/right/input/grip\" \n"
      "           }, \n"
      "           {\n"
      "             \"inputs\" : { \n"
      "               \"position\" : { \n"
      "                   \"output\" : \"/actions/firefox/in/LHand_trackpad_analog\" \n"
      "               }, \n"
      "               \"click\" : { \n"
      "                   \"output\" : \"/actions/firefox/in/LHand_trackpad_pressed\" \n"
      "               }, \n"
      "               \"touch\" : { \n"
      "                   \"output\" : \"/actions/firefox/in/LHand_trackpad_touched\" \n"
      "               } \n"
      "             }, \n"
      "             \"mode\" : \"trackpad\", \n"
      "             \"path\" : \"/user/hand/left/input/trackpad\" \n"
      "           },\n"
      "           {\n"
      "             \"inputs\" : { \n"
      "               \"position\" : { \n"
      "                   \"output\" : \"/actions/firefox/in/RHand_trackpad_analog\" \n"
      "               }, \n"
      "               \"click\" : { \n"
      "                   \"output\" : \"/actions/firefox/in/RHand_trackpad_pressed\" \n"
      "               }, \n"
      "               \"touch\" : { \n"
      "                   \"output\" : \"/actions/firefox/in/RHand_trackpad_touched\" \n"
      "               } \n"
      "             }, \n"
      "             \"mode\" : \"trackpad\", \n"
      "             \"path\" : \"/user/hand/right/input/trackpad\" \n"
      "           }\n"
      "         ]\n"
      "     }\n"
      "  }\n"
      "}";

    std::ofstream viveBindingFile(viveBindingFileName);
    if (viveBindingFile.is_open()) {
      viveBindingFile << vive_controller;
      viveBindingFile.close();
    }

    // Create and open a temp file
    std::string knucklesBindingFileName = std::tmpnam(nullptr);

    // TODO: Write it to profile folder.

    // In order to be more readable when editing this config file, we choose
    // don't use JSONWriter.
    const char* knuckles_controller =
      " \n"
      "{\n"
      "  \"controller_type\" : \"knuckles\", \n"
      "  \"description\" : \"Bindings for Firefox OpenVR for the Knuckles controller\", \n"
      "  \"name\" : \"Firefox bindings for Knuckles Controller\", \n"
      "  \"bindings\" : { \n"
      "     \"/actions/firefox\" : { \n"
      "         \"poses\" : [        \n"
      "           {                \n"
      "             \"output\" : \"/actions/firefox/in/LHand_pose\", \n"
      "              \"path\" : \"/user/hand/left/pose/raw\"     \n"
      "            },\n"
      "            {                                             \n"
      "              \"output\" : \"/actions/firefox/in/RHand_pose\", \n"
      "              \"path\" : \"/user/hand/right/pose/raw\"    \n"
      "            }\n"
      "         ],\n"
      "         \"haptics\" : [ \n"
      "            {\n"
      "              \"output\" : \"/actions/firefox/out/LHand_haptic\",  \n"
      "              \"path\" : \"/user/hand/left/output/haptic\"     \n"
      "            },\n"
      "            { \n"
      "              \"output\" : \"/actions/firefox/out/RHand_haptic\", \n"
      "              \"path\" : \"/user/hand/right/output/haptic\"    \n"
      "            }\n"
      "		  ],\n"
      "       \"sources\" : [ \n"
      "           {\n"
      "             \"inputs\" : { \n"
      "                 \"click\" : { \n"
      "                    \"output\" : \"/actions/firefox/in/LHand_menu_pressed\"  \n"
      "                   }, \n"
      "                 \"touch\" : { \n"
      "                    \"output\" : \"/actions/firefox/in/LHand_menu_touched\"  \n"
      "                   } \n"
      "              },\n"
      "             \"mode\" : \"button\", \n"
      "             \"path\" : \"/user/hand/left/input/application_menu\" \n"
      "           }, \n"
      "           {\n"
      "             \"inputs\" : { \n"
      "                 \"click\" : { \n"
      "                    \"output\" : \"/actions/firefox/in/RHand_menu_pressed\"  \n"
      "                   }, \n"
      "                 \"touch\" : { \n"
      "                    \"output\" : \"/actions/firefox/in/RHand_menu_touched\"  \n"
      "                   } \n"
      "              },\n"
      "             \"mode\" : \"button\", \n"
      "             \"path\" : \"/user/hand/right/input/application_menu\" \n"
      "           }, \n"
      "           {\n"
      "             \"inputs\" : { \n"
      "                 \"pull\" : { \n"
      "                    \"output\" : \"/actions/firefox/in/LHand_trigger_value\"  \n"
      "                   } \n"
      "              },\n"
      "             \"mode\" : \"trigger\", \n"
      "             \"path\" : \"/user/hand/left/input/trigger\" \n"
      "           }, \n"
      "           {\n"
      "             \"inputs\" : { \n"
      "                 \"pull\" : { \n"
      "                    \"output\" : \"/actions/firefox/in/RHand_trigger_value\"  \n"
      "                   } \n"
      "              },\n"
      "             \"mode\" : \"trigger\", \n"
      "             \"path\" : \"/user/hand/right/input/trigger\" \n"
      "           }, \n"
      "           {\n"
      "             \"inputs\" : { \n"
      "               \"click\" : { \n"
      "                \"output\" : \"/actions/firefox/in/LHand_grip_pressed\" \n"
      "              }, \n"
      "               \"touch\" : { \n"
      "                 \"output\" : "
      "                  \"/actions/firefox/in/LHand_grip_touched\" \n"
      "              } \n"
      "             }, \n"
      "				      \"mode\" : \"button\", \n"
      "				      \"path\" : \"/user/hand/left/input/grip\" \n"
      "           }, \n"
      "           {\n"
      "             \"inputs\" : { \n"
      "               \"click\" : { \n"
      "                \"output\" : \"/actions/firefox/in/RHand_grip_pressed\" \n"
      "              }, \n"
      "               \"touch\" : { \n"
      "                 \"output\" : \"/actions/firefox/in/RHand_grip_touched\" \n"
      "              } \n"
      "           }, \n"
      "				      \"mode\" : \"button\", \n"
      "				      \"path\" : \"/user/hand/right/input/grip\" \n"
      "           }, \n"
      "           {\n"
      "             \"inputs\" : { \n"
      "               \"position\" : { \n"
      "                   \"output\" : \"/actions/firefox/in/LHand_trackpad_analog\" \n"
      "               }, \n"
      "               \"click\" : { \n"
      "                   \"output\" : \"/actions/firefox/in/LHand_trackpad_pressed\" \n"
      "               }, \n"
      "               \"touch\" : { \n"
      "                   \"output\" : \"/actions/firefox/in/LHand_trackpad_touched\" \n"
      "               } \n"
      "             }, \n"
      "             \"mode\" : \"trackpad\", \n"
      "             \"path\" : \"/user/hand/left/input/trackpad\" \n"
      "           },\n"
      "           {\n"
      "             \"inputs\" : { \n"
      "               \"position\" : { \n"
      "                   \"output\" : \"/actions/firefox/in/RHand_trackpad_analog\" \n"
      "               }, \n"
      "               \"click\" : { \n"
      "                   \"output\" : \"/actions/firefox/in/RHand_trackpad_pressed\" \n"
      "               }, \n"
      "               \"touch\" : { \n"
      "                   \"output\" : \"/actions/firefox/in/RHand_trackpad_touched\" \n"
      "               } \n"
      "             }, \n"
      "             \"mode\" : \"trackpad\", \n"
      "             \"path\" : \"/user/hand/right/input/trackpad\" \n"
      "           }\n"
      "         ]\n"
      "     }\n"
      "  }\n"
      "}";

    std::ofstream knucklesBindingFile(knucklesBindingFileName);
    if (knucklesBindingFile.is_open()) {
      knucklesBindingFile << knuckles_controller;
      knucklesBindingFile.close();
    }

    std::string actionFileName = std::tmpnam(nullptr);
    nsAutoString actionData;
    JSONWriter actionWriter(MakeUnique<StringWriteFunc>(actionData));
    actionWriter.Start();

    // "default_bindings": []
    actionWriter.StartArrayProperty("default_bindings");
    actionWriter.StartObjectElement();
    actionWriter.StringProperty("controller_type", "vive_controller");
    actionWriter.StringProperty("binding_url", viveBindingFileName.c_str());
    actionWriter.EndObject();
    actionWriter.StartObjectElement();
    actionWriter.StringProperty("controller_type", "knuckles");
    actionWriter.StringProperty("binding_url", knucklesBindingFileName.c_str());
    actionWriter.EndObject();
    actionWriter.EndArray(); // End "default_bindings": []

    // "actions": [] Action paths must take the form: "/actions/<action set>/in|out/<action>"
    actionWriter.StartArrayProperty("actions");
    // Left hand
    actionWriter.StartObjectElement();
    actionWriter.StringProperty("name", "/actions/firefox/in/LHand_pose");
    actionWriter.StringProperty("type", "pose");
    actionWriter.EndObject();
    actionWriter.StartObjectElement();
    actionWriter.StringProperty("name",
                                "/actions/firefox/in/LHand_trackpad_pressed");
    actionWriter.StringProperty("type", "boolean");
    actionWriter.EndObject();
    actionWriter.StartObjectElement();
    actionWriter.StringProperty("name",
                                "/actions/firefox/in/LHand_trackpad_touched");
    actionWriter.StringProperty("type", "boolean");
    actionWriter.EndObject();
    actionWriter.StartObjectElement();
    actionWriter.StringProperty("name",
                                "/actions/firefox/in/LHand_trackpad_analog");
    actionWriter.StringProperty("type", "vector2");
    actionWriter.EndObject();
    actionWriter.StartObjectElement();
    actionWriter.StringProperty("name",
                                "/actions/firefox/in/LHand_trigger_value");
    actionWriter.StringProperty("type", "vector1");
    actionWriter.EndObject();
    actionWriter.StartObjectElement();
    actionWriter.StringProperty("name",
                                "/actions/firefox/in/LHand_grip_pressed");
    actionWriter.StringProperty("type", "boolean");
    actionWriter.EndObject();
    actionWriter.StartObjectElement();
    actionWriter.StringProperty("name",
                                "/actions/firefox/in/LHand_grip_touched");
    actionWriter.StringProperty("type", "boolean");
    actionWriter.EndObject();
    actionWriter.StartObjectElement();
    actionWriter.StringProperty("name",
                                "/actions/firefox/in/LHand_menu_pressed");
    actionWriter.StringProperty("type", "boolean");
    actionWriter.EndObject();
    actionWriter.StartObjectElement();
    actionWriter.StringProperty("name",
                                "/actions/firefox/in/LHand_menu_touched");
    actionWriter.StringProperty("type", "boolean");
    actionWriter.EndObject();
    actionWriter.StartObjectElement();
    actionWriter.StringProperty("name", "/actions/firefox/out/LHand_haptic");
    actionWriter.StringProperty("type", "vibration");
    actionWriter.EndObject();

    // Right hand
    actionWriter.StartObjectElement();
    actionWriter.StringProperty("name", "/actions/firefox/in/RHand_pose");
    actionWriter.StringProperty("type", "pose");
    actionWriter.EndObject();
    actionWriter.StartObjectElement();
    actionWriter.StringProperty("name",
                                "/actions/firefox/in/RHand_trackpad_pressed");
    actionWriter.StringProperty("type", "boolean");
    actionWriter.EndObject();
    actionWriter.StartObjectElement();
    actionWriter.StringProperty("name",
                                "/actions/firefox/in/RHand_trackpad_touched");
    actionWriter.StringProperty("type", "boolean");
    actionWriter.EndObject();
    actionWriter.StartObjectElement();
    actionWriter.StringProperty("name",
                                "/actions/firefox/in/RHand_trackpad_analog");
    actionWriter.StringProperty("type", "vector2");
    actionWriter.EndObject();
    actionWriter.StartObjectElement();
    actionWriter.StringProperty("name",
                                "/actions/firefox/in/RHand_trigger_value");
    actionWriter.StringProperty("type", "vector1");
    actionWriter.EndObject();
    actionWriter.StartObjectElement();
    actionWriter.StringProperty("name",
                                "/actions/firefox/in/RHand_grip_pressed");
    actionWriter.StringProperty("type", "boolean");
    actionWriter.EndObject();
    actionWriter.StartObjectElement();
    actionWriter.StringProperty("name",
                                "/actions/firefox/in/RHand_grip_touched");
    actionWriter.StringProperty("type", "boolean");
    actionWriter.EndObject();
    actionWriter.StartObjectElement();
    actionWriter.StringProperty("name",
                                "/actions/firefox/in/RHand_menu_pressed");
    actionWriter.StringProperty("type", "boolean");
    actionWriter.EndObject();
    actionWriter.StartObjectElement();
    actionWriter.StringProperty("name",
                                "/actions/firefox/in/RHand_menu_touched");
    actionWriter.StringProperty("type", "boolean");
    actionWriter.EndObject();
    actionWriter.StartObjectElement();
    actionWriter.StringProperty("name", "/actions/firefox/out/RHand_haptic");
    actionWriter.StringProperty("type", "vibration");
    actionWriter.EndObject();
    actionWriter.EndArray(); // End "actions": []

    actionWriter.End();

    std::ofstream actionfile(actionFileName);
    nsCString actionResult(NS_ConvertUTF16toUTF8(actionData.get()));
    if (actionfile.is_open()) {
      actionfile << actionResult.get();
      actionfile.close();
    }

    // Test for reading file.
    // char buffer[256];
    // std::ifstream examplefileRead (actionFileName);
    // if (!examplefileRead.is_open())
    //{ printf_stderr("error...\n"); }

    // while (! examplefileRead.eof() )
    //{
    //  examplefileRead.getline (buffer,100);
    //  printf_stderr("buff...%s\n", buffer);
    //}
    // examplefileRead.close();

    // Setting controller actions
    //const char* path = "C:/Projects/openvr/samples/bin/hellovr_actions.json";
    vr::VRInput()->SetActionManifestPath(actionFileName.c_str());

    m_actionsetFirefox = vr::k_ulInvalidActionSetHandle;
    vr::VRInput()->GetActionSetHandle("/actions/firefox", &m_actionsetFirefox);
    vr::VRInput()->GetInputSourceHandle("/user/hand/left",
                                        &m_rHand[Left].m_source);
    vr::VRInput()->GetActionHandle("/actions/firefox/in/LHand_pose",
                                   &m_rHand[Left].m_actionPose);
    vr::VRInput()->GetActionHandle("/actions/firefox/out/LHand_haptic",
                                   &m_rHand[Left].m_actionHaptic);
    vr::VRInput()->GetActionHandle("/actions/firefox/in/LHand_trackpad_analog",
                                   &m_rHand[Left].m_actionTrackpad_Analog);
    vr::VRInput()->GetActionHandle("/actions/firefox/in/LHand_trackpad_pressed",
                                   &m_rHand[Left].m_actionTrackpad_Pressed);
    vr::VRInput()->GetActionHandle("/actions/firefox/in/LHand_trackpad_touched",
                                   &m_rHand[Left].m_actionTrackpad_Touched);
    vr::VRInput()->GetActionHandle("/actions/firefox/in/LHand_trigger_value",
                                   &m_rHand[Left].m_actionTrigger_Value);
    vr::VRInput()->GetActionHandle("/actions/firefox/in/LHand_grip_pressed",
                                   &m_rHand[Left].m_actionGrip_Pressed);
    vr::VRInput()->GetActionHandle("/actions/firefox/in/LHand_grip_touched",
                                   &m_rHand[Left].m_actionGrip_Touched);
    vr::VRInput()->GetActionHandle("/actions/firefox/in/LHand_menu_pressed",
                                   &m_rHand[Left].m_actionMenu_Pressed);
    vr::VRInput()->GetActionHandle("/actions/firefox/in/LHand_menu_touched",
                                   &m_rHand[Left].m_actionMenu_Touched);

    vr::VRInput()->GetInputSourceHandle("/user/hand/right",
                                        &m_rHand[Right].m_source);
    vr::VRInput()->GetActionHandle("/actions/firefox/in/RHand_pose",
                                   &m_rHand[Right].m_actionPose);
    vr::VRInput()->GetActionHandle("/actions/firefox/out/RHand_haptic",
                                   &m_rHand[Right].m_actionHaptic);
    vr::VRInput()->GetActionHandle("/actions/firefox/in/RHand_trackpad_analog",
                                   &m_rHand[Right].m_actionTrackpad_Analog);
    vr::VRInput()->GetActionHandle("/actions/firefox/in/RHand_trackpad_pressed",
                                   &m_rHand[Right].m_actionTrackpad_Pressed);
    vr::VRInput()->GetActionHandle("/actions/firefox/in/RHand_trackpad_touched",
                                   &m_rHand[Right].m_actionTrackpad_Touched);
    vr::VRInput()->GetActionHandle("/actions/firefox/in/RHand_trigger_value",
                                   &m_rHand[Right].m_actionTrigger_Value);
    vr::VRInput()->GetActionHandle("/actions/firefox/in/RHand_grip_pressed",
                                   &m_rHand[Right].m_actionGrip_Pressed);
    vr::VRInput()->GetActionHandle("/actions/firefox/in/RHand_grip_touched",
                                   &m_rHand[Right].m_actionGrip_Touched);
    vr::VRInput()->GetActionHandle("/actions/firefox/in/RHand_menu_pressed",
                                   &m_rHand[Right].m_actionMenu_Pressed);
    vr::VRInput()->GetActionHandle("/actions/firefox/in/RHand_menu_touched",
                                   &m_rHand[Right].m_actionMenu_Touched);

    // char buf[256];
    //// Write someting to the action file
    //// Creating a temporary file in memory for the action file.
    // FILE* pf = NULL;

    // if (pf) {
    //  fputs("This is a temp file", pf);  // Write some data to it
    //}

    // fseek(pf, 5, SEEK_SET);  // Reset the file position
    // fgets(buf, 255, pf);     // Read a string from it
    // fclose(pf);

    // printf_stderr("the buffer is %s\n", buf);
  }

  StartHapticThread();
  StartHapticTimer();

  // Succeeded
  return true;
}

#if defined(XP_WIN)
bool
OpenVRSession::CreateD3DObjects()
{
  RefPtr<ID3D11Device> device = gfx::DeviceManagerDx::Get()->GetVRDevice();
  if (!device) {
    return false;
  }
  if (!CreateD3DContext(device)) {
    return false;
  }
  return true;
}
#endif

void
OpenVRSession::Shutdown()
{
  StopHapticTimer();
  StopHapticThread();
  if (mVRSystem || mVRCompositor || mVRSystem) {
    ::vr::VR_Shutdown();
    mVRCompositor = nullptr;
    mVRChaperone = nullptr;
    mVRSystem = nullptr;
  }
}

bool
OpenVRSession::InitState(VRSystemState& aSystemState)
{
  VRDisplayState& state = aSystemState.displayState;
  strncpy(state.mDisplayName, "OpenVR HMD", kVRDisplayNameMaxLen);
  state.mEightCC = GFX_VR_EIGHTCC('O', 'p', 'e', 'n', 'V', 'R', ' ', ' ');
  state.mIsConnected =
    mVRSystem->IsTrackedDeviceConnected(::vr::k_unTrackedDeviceIndex_Hmd);
  state.mIsMounted = false;
  state.mCapabilityFlags = (VRDisplayCapabilityFlags)(
    (int)VRDisplayCapabilityFlags::Cap_None |
    (int)VRDisplayCapabilityFlags::Cap_Orientation |
    (int)VRDisplayCapabilityFlags::Cap_Position |
    (int)VRDisplayCapabilityFlags::Cap_External |
    (int)VRDisplayCapabilityFlags::Cap_Present |
    (int)VRDisplayCapabilityFlags::Cap_StageParameters);
  state.mReportsDroppedFrames = true;

  ::vr::ETrackedPropertyError err;
  bool bHasProximitySensor = mVRSystem->GetBoolTrackedDeviceProperty(
    ::vr::k_unTrackedDeviceIndex_Hmd,
    ::vr::Prop_ContainsProximitySensor_Bool,
    &err);
  if (err == ::vr::TrackedProp_Success && bHasProximitySensor) {
    state.mCapabilityFlags = (VRDisplayCapabilityFlags)(
      (int)state.mCapabilityFlags |
      (int)VRDisplayCapabilityFlags::Cap_MountDetection);
  }

  uint32_t w, h;
  mVRSystem->GetRecommendedRenderTargetSize(&w, &h);
  state.mEyeResolution.width = w;
  state.mEyeResolution.height = h;

  // default to an identity quaternion
  aSystemState.sensorState.pose.orientation[3] = 1.0f;

  UpdateStageParameters(state);
  UpdateEyeParameters(aSystemState);

  VRHMDSensorState& sensorState = aSystemState.sensorState;
  sensorState.flags =
    (VRDisplayCapabilityFlags)((int)VRDisplayCapabilityFlags::Cap_Orientation |
                               (int)VRDisplayCapabilityFlags::Cap_Position);
  sensorState.pose.orientation[3] = 1.0f; // Default to an identity quaternion

  return true;
}

void
OpenVRSession::UpdateStageParameters(VRDisplayState& aState)
{
  float sizeX = 0.0f;
  float sizeZ = 0.0f;
  if (mVRChaperone->GetPlayAreaSize(&sizeX, &sizeZ)) {
    ::vr::HmdMatrix34_t t =
      mVRSystem->GetSeatedZeroPoseToStandingAbsoluteTrackingPose();
    aState.mStageSize.width = sizeX;
    aState.mStageSize.height = sizeZ;

    aState.mSittingToStandingTransform[0] = t.m[0][0];
    aState.mSittingToStandingTransform[1] = t.m[1][0];
    aState.mSittingToStandingTransform[2] = t.m[2][0];
    aState.mSittingToStandingTransform[3] = 0.0f;

    aState.mSittingToStandingTransform[4] = t.m[0][1];
    aState.mSittingToStandingTransform[5] = t.m[1][1];
    aState.mSittingToStandingTransform[6] = t.m[2][1];
    aState.mSittingToStandingTransform[7] = 0.0f;

    aState.mSittingToStandingTransform[8] = t.m[0][2];
    aState.mSittingToStandingTransform[9] = t.m[1][2];
    aState.mSittingToStandingTransform[10] = t.m[2][2];
    aState.mSittingToStandingTransform[11] = 0.0f;

    aState.mSittingToStandingTransform[12] = t.m[0][3];
    aState.mSittingToStandingTransform[13] = t.m[1][3];
    aState.mSittingToStandingTransform[14] = t.m[2][3];
    aState.mSittingToStandingTransform[15] = 1.0f;
  } else {
    // If we fail, fall back to reasonable defaults.
    // 1m x 1m space, 0.75m high in seated position
    aState.mStageSize.width = 1.0f;
    aState.mStageSize.height = 1.0f;

    aState.mSittingToStandingTransform[0] = 1.0f;
    aState.mSittingToStandingTransform[1] = 0.0f;
    aState.mSittingToStandingTransform[2] = 0.0f;
    aState.mSittingToStandingTransform[3] = 0.0f;

    aState.mSittingToStandingTransform[4] = 0.0f;
    aState.mSittingToStandingTransform[5] = 1.0f;
    aState.mSittingToStandingTransform[6] = 0.0f;
    aState.mSittingToStandingTransform[7] = 0.0f;

    aState.mSittingToStandingTransform[8] = 0.0f;
    aState.mSittingToStandingTransform[9] = 0.0f;
    aState.mSittingToStandingTransform[10] = 1.0f;
    aState.mSittingToStandingTransform[11] = 0.0f;

    aState.mSittingToStandingTransform[12] = 0.0f;
    aState.mSittingToStandingTransform[13] = 0.75f;
    aState.mSittingToStandingTransform[14] = 0.0f;
    aState.mSittingToStandingTransform[15] = 1.0f;
  }
}

void
OpenVRSession::UpdateEyeParameters(VRSystemState& aState)
{
  // This must be called every frame in order to
  // account for continuous adjustments to ipd.
  gfx::Matrix4x4 headToEyeTransforms[2];

  for (uint32_t eye = 0; eye < 2; ++eye) {
    ::vr::HmdMatrix34_t eyeToHead =
      mVRSystem->GetEyeToHeadTransform(static_cast<::vr::Hmd_Eye>(eye));
    aState.displayState.mEyeTranslation[eye].x = eyeToHead.m[0][3];
    aState.displayState.mEyeTranslation[eye].y = eyeToHead.m[1][3];
    aState.displayState.mEyeTranslation[eye].z = eyeToHead.m[2][3];

    float left, right, up, down;
    mVRSystem->GetProjectionRaw(
      static_cast<::vr::Hmd_Eye>(eye), &left, &right, &up, &down);
    aState.displayState.mEyeFOV[eye].upDegrees = atan(-up) * 180.0 / M_PI;
    aState.displayState.mEyeFOV[eye].rightDegrees = atan(right) * 180.0 / M_PI;
    aState.displayState.mEyeFOV[eye].downDegrees = atan(down) * 180.0 / M_PI;
    aState.displayState.mEyeFOV[eye].leftDegrees = atan(-left) * 180.0 / M_PI;

    Matrix4x4 pose;
    // NOTE! eyeToHead.m is a 3x4 matrix, not 4x4.  But
    // because of its arrangement, we can copy the 12 elements in and
    // then transpose them to the right place.
    memcpy(&pose._11, &eyeToHead.m, sizeof(eyeToHead.m));
    pose.Transpose();
    pose.Invert();
    headToEyeTransforms[eye] = pose;
  }
  aState.sensorState.CalcViewMatrices(headToEyeTransforms);
}

void
OpenVRSession::UpdateHeadsetPose(VRSystemState& aState)
{
  const uint32_t posesSize = ::vr::k_unTrackedDeviceIndex_Hmd + 1;
  ::vr::TrackedDevicePose_t poses[posesSize];
  // Note: We *must* call WaitGetPoses in order for any rendering to happen at
  // all.
  mVRCompositor->WaitGetPoses(poses, posesSize, nullptr, 0);

  ::vr::Compositor_FrameTiming timing;
  timing.m_nSize = sizeof(::vr::Compositor_FrameTiming);
  if (mVRCompositor->GetFrameTiming(&timing)) {
    aState.sensorState.timestamp = timing.m_flSystemTimeInSeconds;
  } else {
    // This should not happen, but log it just in case
    fprintf(stderr, "OpenVR - IVRCompositor::GetFrameTiming failed");
  }

  if (poses[::vr::k_unTrackedDeviceIndex_Hmd].bDeviceIsConnected &&
      poses[::vr::k_unTrackedDeviceIndex_Hmd].bPoseIsValid &&
      poses[::vr::k_unTrackedDeviceIndex_Hmd].eTrackingResult ==
        ::vr::TrackingResult_Running_OK) {
    const ::vr::TrackedDevicePose_t& pose =
      poses[::vr::k_unTrackedDeviceIndex_Hmd];

    gfx::Matrix4x4 m;
    // NOTE! mDeviceToAbsoluteTracking is a 3x4 matrix, not 4x4.  But
    // because of its arrangement, we can copy the 12 elements in and
    // then transpose them to the right place.  We do this so we can
    // pull out a Quaternion.
    memcpy(&m._11,
           &pose.mDeviceToAbsoluteTracking,
           sizeof(pose.mDeviceToAbsoluteTracking));
    m.Transpose();

    gfx::Quaternion rot;
    rot.SetFromRotationMatrix(m);
    rot.Invert();

    aState.sensorState.flags = (VRDisplayCapabilityFlags)(
      (int)aState.sensorState.flags |
      (int)VRDisplayCapabilityFlags::Cap_Orientation);
    aState.sensorState.pose.orientation[0] = rot.x;
    aState.sensorState.pose.orientation[1] = rot.y;
    aState.sensorState.pose.orientation[2] = rot.z;
    aState.sensorState.pose.orientation[3] = rot.w;
    aState.sensorState.pose.angularVelocity[0] = pose.vAngularVelocity.v[0];
    aState.sensorState.pose.angularVelocity[1] = pose.vAngularVelocity.v[1];
    aState.sensorState.pose.angularVelocity[2] = pose.vAngularVelocity.v[2];

    aState.sensorState.flags =
      (VRDisplayCapabilityFlags)((int)aState.sensorState.flags |
                                 (int)VRDisplayCapabilityFlags::Cap_Position);
    aState.sensorState.pose.position[0] = m._41;
    aState.sensorState.pose.position[1] = m._42;
    aState.sensorState.pose.position[2] = m._43;
    aState.sensorState.pose.linearVelocity[0] = pose.vVelocity.v[0];
    aState.sensorState.pose.linearVelocity[1] = pose.vVelocity.v[1];
    aState.sensorState.pose.linearVelocity[2] = pose.vVelocity.v[2];
  }
}

void
OpenVRSession::EnumerateControllers(VRSystemState& aState)
{
  MOZ_ASSERT(mVRSystem);

  MutexAutoLock lock(mControllerHapticStateMutex);

  bool controllerPresent[kVRControllerMaxCount] = { false };

  // Basically, we would have HMDs in the tracked devices,
  // but we are just interested in the controllers.
  for (::vr::TrackedDeviceIndex_t trackedDevice =
         ::vr::k_unTrackedDeviceIndex_Hmd + 1;
       trackedDevice < ::vr::k_unMaxTrackedDeviceCount;
       ++trackedDevice) {

    if (!mVRSystem->IsTrackedDeviceConnected(trackedDevice)) {
      continue;
    }

    const ::vr::ETrackedDeviceClass deviceType =
      mVRSystem->GetTrackedDeviceClass(trackedDevice);
    if (deviceType != ::vr::TrackedDeviceClass_Controller &&
        deviceType != ::vr::TrackedDeviceClass_GenericTracker) {
      continue;
    }

    uint32_t stateIndex = 0;
    uint32_t firstEmptyIndex = kVRControllerMaxCount;

    // Find the existing controller
    for (stateIndex = 0; stateIndex < kVRControllerMaxCount; stateIndex++) {
      if (mControllerDeviceIndex[stateIndex] == 0 &&
          firstEmptyIndex == kVRControllerMaxCount) {
        firstEmptyIndex = stateIndex;
      }
      if (mControllerDeviceIndex[stateIndex] == trackedDevice) {
        break;
      }
    }
    if (stateIndex == kVRControllerMaxCount) {
      // This is a new controller, let's add it
      if (firstEmptyIndex == kVRControllerMaxCount) {
        NS_WARNING("OpenVR - Too many controllers, need to increase "
                   "kVRControllerMaxCount.");
        continue;
      }
      stateIndex = firstEmptyIndex;
      mControllerDeviceIndex[stateIndex] = trackedDevice;
      VRControllerState& controllerState = aState.controllerState[stateIndex];
      uint32_t numButtons = 0;
      uint32_t numAxes = 0;

      // Scan the axes that the controllers support
      for (uint32_t j = 0; j < ::vr::k_unControllerStateAxisCount; ++j) {
        const uint32_t supportAxis = mVRSystem->GetInt32TrackedDeviceProperty(
          trackedDevice,
          static_cast<vr::TrackedDeviceProperty>(::vr::Prop_Axis0Type_Int32 +
                                                 j));
        switch (supportAxis) {
          case ::vr::EVRControllerAxisType::k_eControllerAxis_Joystick:
          case ::vr::EVRControllerAxisType::k_eControllerAxis_TrackPad:
            numAxes += 2; // It has x and y axes.
            ++numButtons;
            break;
          case ::vr::k_eControllerAxis_Trigger:
            if (j <= 2) {
              ++numButtons;
            } else {
#ifdef DEBUG
              // SteamVR Knuckles is the only special case for using 2D axis
              // values on triggers.
              ::vr::ETrackedPropertyError err;
              uint32_t requiredBufferLen;
              char charBuf[128];
              requiredBufferLen = mVRSystem->GetStringTrackedDeviceProperty(
                trackedDevice,
                ::vr::Prop_RenderModelName_String,
                charBuf,
                128,
                &err);
              MOZ_ASSERT(requiredBufferLen && err == ::vr::TrackedProp_Success);
              nsCString deviceId(charBuf);
              MOZ_ASSERT(deviceId.Find("knuckles") != kNotFound);
#endif // #ifdef DEBUG
              numButtons += 2;
            }
            break;
        }
      }

      // Scan the buttons that the controllers support
      const uint64_t supportButtons = mVRSystem->GetUint64TrackedDeviceProperty(
        trackedDevice, ::vr::Prop_SupportedButtons_Uint64);
      if (supportButtons & BTN_MASK_FROM_ID(k_EButton_A)) {
        ++numButtons;
      }
      if (supportButtons & BTN_MASK_FROM_ID(k_EButton_Grip)) {
        ++numButtons;
      }
      if (supportButtons & BTN_MASK_FROM_ID(k_EButton_ApplicationMenu)) {
        ++numButtons;
      }
      if (supportButtons & BTN_MASK_FROM_ID(k_EButton_DPad_Left)) {
        ++numButtons;
      }
      if (supportButtons & BTN_MASK_FROM_ID(k_EButton_DPad_Up)) {
        ++numButtons;
      }
      if (supportButtons & BTN_MASK_FROM_ID(k_EButton_DPad_Right)) {
        ++numButtons;
      }
      if (supportButtons & BTN_MASK_FROM_ID(k_EButton_DPad_Down)) {
        ++numButtons;
      }

      nsCString deviceId;
      GetControllerDeviceId(deviceType, trackedDevice, deviceId);

      strncpy(controllerState.controllerName,
              deviceId.BeginReading(),
              kVRControllerNameMaxLen);
      controllerState.numButtons = numButtons;
      controllerState.numAxes = numAxes;
      controllerState.numHaptics = kNumOpenVRHaptics;

      // If the Windows MR controller doesn't has the amount
      // of buttons or axes as our expectation, switching off
      // the workaround for Windows MR.
      if (mIsWindowsMR && (numAxes < 4 || numButtons < 5)) {
        mIsWindowsMR = false;
        NS_WARNING("OpenVR - Switching off Windows MR mode.");
      }
    }
    controllerPresent[stateIndex] = true;
  }
  // Clear out entries for disconnected controllers
  for (uint32_t stateIndex = 0; stateIndex < kVRControllerMaxCount;
       stateIndex++) {
    if (!controllerPresent[stateIndex] &&
        mControllerDeviceIndex[stateIndex] != 0) {
      mControllerDeviceIndex[stateIndex] = 0;
      memset(&aState.controllerState[stateIndex], 0, sizeof(VRControllerState));
    }
  }
}

void
OpenVRSession::UpdateControllerButtons(VRSystemState& aState)
{
  MOZ_ASSERT(mVRSystem);

  // Compared to Edge, we have a wrong implementation for the vertical axis
  // value. In order to not affect the current VR content, we add a workaround
  // for yAxis.
  const float yAxisInvert = (mIsWindowsMR) ? -1.0f : 1.0f;
  const float triggerThreshold = gfxPrefs::VRControllerTriggerThreshold();

  for (uint32_t stateIndex = 0; stateIndex < kVRControllerMaxCount;
       stateIndex++) {
    ::vr::TrackedDeviceIndex_t trackedDevice =
      mControllerDeviceIndex[stateIndex];
    if (trackedDevice == 0) {
      continue;
    }
    VRControllerState& controllerState = aState.controllerState[stateIndex];
    const ::vr::ETrackedControllerRole role =
      mVRSystem->GetControllerRoleForTrackedDeviceIndex(trackedDevice);
    dom::GamepadHand hand = GetControllerHandFromControllerRole(role);
    controllerState.hand = hand;

    uint32_t axisIdx = 0;
    uint32_t buttonIdx = 0;
    // Axis 0 1: Trackpad
    // Button 0: Trackpad
    vr::InputAnalogActionData_t analogData;
    if (vr::VRInput()->GetAnalogActionData(
          m_rHand[stateIndex].m_actionTrackpad_Analog,
          &analogData,
          sizeof(analogData),
          vr::k_ulInvalidInputValueHandle) == vr::VRInputError_None &&
        analogData.bActive) {
      controllerState.axisValue[axisIdx] = analogData.x;
      ++axisIdx;
      controllerState.axisValue[axisIdx] = analogData.y * yAxisInvert;
      ++axisIdx;
    }
    vr::InputDigitalActionData_t actionData;
    bool bPressed = false;
    bool bTouched = false;
    vr::VRInput()->GetDigitalActionData(
      m_rHand[stateIndex].m_actionTrackpad_Pressed,
      &actionData,
      sizeof(actionData),
      vr::k_ulInvalidInputValueHandle);
    bPressed = actionData.bActive && actionData.bState;
    //  UpdateButton(controllerState, vrControllerState, buttonIdx, buttonMask);
    uint64_t mask = (1ULL << buttonIdx);
    controllerState.triggerValue[buttonIdx] = bPressed ? 1.0 : 0.0f;
    if (bPressed) {
      controllerState.buttonPressed |= mask;
    } else {
      controllerState.buttonPressed &= ~mask;
    }

    vr::VRInput()->GetDigitalActionData(
      m_rHand[stateIndex].m_actionTrackpad_Touched,
      &actionData,
      sizeof(actionData),
      vr::k_ulInvalidInputValueHandle);
    // vr::VRInputValueHandle_t ulTouched;
    // if (GetDigitalActionState(m_rHand[stateIndex].m_actionTrackpad_Touched,
    // &ulTouched)) {
    bTouched = actionData.bActive && actionData.bState;
    mask = (1ULL << buttonIdx);
    if (bTouched) {
      controllerState.buttonTouched |= mask;
    } else {
      controllerState.buttonTouched &= ~mask;
    }
    // }
    ++buttonIdx;

    // Button 1: Trigger
    if (vr::VRInput()->GetAnalogActionData(
          m_rHand[stateIndex].m_actionTrigger_Value,
          &analogData,
          sizeof(analogData),
          vr::k_ulInvalidInputValueHandle) == vr::VRInputError_None &&
        analogData.bActive) {
      UpdateTrigger(controllerState, buttonIdx, analogData.x, triggerThreshold);
    }
    ++buttonIdx;
    // Button 2: Grip
    // vr::VRInputValueHandle_t ulPressed;
    vr::VRInput()->GetDigitalActionData(
      m_rHand[stateIndex].m_actionGrip_Pressed,
      &actionData,
      sizeof(actionData),
      vr::k_ulInvalidInputValueHandle);
    // if (GetDigitalActionState(m_rHand[stateIndex].m_actionGrip_Pressed,
    // &ulPressed)) {
    //  UpdateButton(controllerState, vrControllerState, buttonIdx, buttonMask);
    bPressed = actionData.bActive && actionData.bState;
    mask = (1ULL << buttonIdx);
    controllerState.triggerValue[buttonIdx] = bPressed ? 1.0 : 0.0f;
    if (bPressed) {
      controllerState.buttonPressed |= mask;
    } else {
      controllerState.buttonPressed &= ~mask;
    }
    //  }
    vr::VRInput()->GetDigitalActionData(
      m_rHand[stateIndex].m_actionGrip_Touched,
      &actionData,
      sizeof(actionData),
      vr::k_ulInvalidInputValueHandle);
    // vr::VRInputValueHandle_t ulTouched;
    // if (GetDigitalActionState(m_rHand[stateIndex].m_actionGrip_Touched,
    // &ulTouched)) {
    bTouched = actionData.bActive && actionData.bState;
    mask = (1ULL << buttonIdx);
    if (bTouched) {
      controllerState.buttonTouched |= mask;
    } else {
      controllerState.buttonTouched &= ~mask;
    }
    //}
    ++buttonIdx;
    // Button 3: Menu
    // vr::VRInputValueHandle_t ulPressed;
    vr::VRInput()->GetDigitalActionData(
      m_rHand[stateIndex].m_actionMenu_Pressed,
      &actionData,
      sizeof(actionData),
      vr::k_ulInvalidInputValueHandle);
    // if (GetDigitalActionState(m_rHand[stateIndex].m_actionMenu_Pressed,
    // &ulPressed)) {
    //  UpdateButton(controllerState, vrControllerState, buttonIdx, buttonMask);
    bPressed = actionData.bActive && actionData.bState;
    mask = (1ULL << buttonIdx);
    controllerState.triggerValue[buttonIdx] = bPressed ? 1.0 : 0.0f;
    if (bPressed) {
      controllerState.buttonPressed |= mask;
    } else {
      controllerState.buttonPressed &= ~mask;
    }
    //  }
    //  vr::VRInputValueHandle_t ulTouched;
    vr::VRInput()->GetDigitalActionData(
      m_rHand[stateIndex].m_actionMenu_Touched,
      &actionData,
      sizeof(actionData),
      vr::k_ulInvalidInputValueHandle);
    // if (GetDigitalActionState(m_rHand[stateIndex].m_actionMenu_Touched,
    // &ulTouched)) {
    bTouched = actionData.bActive && actionData.bState;
    mask = (1ULL << buttonIdx);
    if (bTouched) {
      controllerState.buttonTouched |= mask;
    } else {
      controllerState.buttonTouched &= ~mask;
    }
    //}
    ++buttonIdx;
  }

  //::vr::VRControllerState_t vrControllerState;
  // if (mVRSystem->GetControllerState(trackedDevice, &vrControllerState,
  // sizeof(vrControllerState))) {
  //  uint32_t axisIdx = 0;
  //  uint32_t buttonIdx = 0;
  //  for (uint32_t j = 0; j < ::vr::k_unControllerStateAxisCount; ++j) {
  //    const uint32_t axisType = mVRSystem->GetInt32TrackedDeviceProperty(
  //                               trackedDevice,
  //                               static_cast<::vr::TrackedDeviceProperty>(
  //                               ::vr::Prop_Axis0Type_Int32 + j));
  //    switch (axisType) {
  //      case ::vr::EVRControllerAxisType::k_eControllerAxis_Joystick:
  //      case ::vr::EVRControllerAxisType::k_eControllerAxis_TrackPad:
  //      {
  //        if (mIsWindowsMR) {
  //          // Adjust the input mapping for Windows MR which has
  //          // different order.
  //          axisIdx = (axisIdx == 0) ? 2 : 0;
  //          buttonIdx = (buttonIdx == 0) ? 4 : 0;
  //        }

  //        controllerState.axisValue[axisIdx] = vrControllerState.rAxis[j].x;
  //        ++axisIdx;
  //        controllerState.axisValue[axisIdx] = vrControllerState.rAxis[j].y *
  //        yAxisInvert;
  //        ++axisIdx;
  //        uint64_t buttonMask = ::vr::ButtonMaskFromId(
  //                             static_cast<::vr::EVRButtonId>(::vr::k_EButton_Axis0
  //                             + j));

  //        UpdateButton(controllerState, vrControllerState, buttonIdx,
  //        buttonMask);
  //        ++buttonIdx;

  //        if (mIsWindowsMR) {
  //          axisIdx = (axisIdx == 4) ? 2 : 4;
  //          buttonIdx = (buttonIdx == 5) ? 1 : 2;
  //        }
  //        break;
  //      }
  //      case vr::EVRControllerAxisType::k_eControllerAxis_Trigger:
  //      {
  //        if (j <= 2) {
  //          UpdateTrigger(controllerState, buttonIdx,
  //          vrControllerState.rAxis[j].x, triggerThreshold);
  //          ++buttonIdx;
  //        } else {
  //          // For SteamVR Knuckles.
  //          UpdateTrigger(controllerState, buttonIdx,
  //          vrControllerState.rAxis[j].x, triggerThreshold);
  //          ++buttonIdx;
  //          UpdateTrigger(controllerState, buttonIdx,
  //          vrControllerState.rAxis[j].y, triggerThreshold);
  //          ++buttonIdx;
  //        }
  //        break;
  //      }
  //    }
  //  }

  //  const uint64_t supportedButtons =
  //  mVRSystem->GetUint64TrackedDeviceProperty(
  //                                     trackedDevice,
  //                                     ::vr::Prop_SupportedButtons_Uint64);
  //  if (supportedButtons &
  //      BTN_MASK_FROM_ID(k_EButton_A)) {
  //    UpdateButton(controllerState, vrControllerState, buttonIdx,
  //    BTN_MASK_FROM_ID(k_EButton_A));
  //    ++buttonIdx;
  //  }
  //  if (supportedButtons &
  //      BTN_MASK_FROM_ID(k_EButton_Grip)) {
  //    UpdateButton(controllerState, vrControllerState, buttonIdx,
  //    BTN_MASK_FROM_ID(k_EButton_Grip));
  //    ++buttonIdx;
  //  }
  //  if (supportedButtons &
  //      BTN_MASK_FROM_ID(k_EButton_ApplicationMenu)) {
  //    UpdateButton(controllerState, vrControllerState, buttonIdx,
  //    BTN_MASK_FROM_ID(k_EButton_ApplicationMenu));
  //    ++buttonIdx;
  //  }
  //  if (mIsWindowsMR) {
  //    // button 4 in Windows MR has already been assigned
  //    // to k_eControllerAxis_TrackPad.
  //    ++buttonIdx;
  //  }
  //  if (supportedButtons &
  //      BTN_MASK_FROM_ID(k_EButton_DPad_Left)) {
  //    UpdateButton(controllerState, vrControllerState, buttonIdx,
  //    BTN_MASK_FROM_ID(k_EButton_DPad_Left));
  //    ++buttonIdx;
  //  }
  //  if (supportedButtons &
  //      BTN_MASK_FROM_ID(k_EButton_DPad_Up)) {
  //    UpdateButton(controllerState, vrControllerState, buttonIdx,
  //    BTN_MASK_FROM_ID(k_EButton_DPad_Up));
  //    ++buttonIdx;
  //  }
  //  if (supportedButtons &
  //      BTN_MASK_FROM_ID(k_EButton_DPad_Right)) {
  //    UpdateButton(controllerState, vrControllerState, buttonIdx,
  //    BTN_MASK_FROM_ID(k_EButton_DPad_Right));
  //    ++buttonIdx;
  //  }
  //  if (supportedButtons &
  //      BTN_MASK_FROM_ID(k_EButton_DPad_Down)) {
  //    UpdateButton(controllerState, vrControllerState, buttonIdx,
  //    BTN_MASK_FROM_ID(k_EButton_DPad_Down));
  //    ++buttonIdx;
  //  }
  // }
  // }
}

void
OpenVRSession::UpdateControllerButtonsObsolete(VRSystemState& aState)
{
  MOZ_ASSERT(mVRSystem);

  // Compared to Edge, we have a wrong implementation for the vertical axis
  // value. In order to not affect the current VR content, we add a workaround
  // for yAxis.
  const float yAxisInvert = (mIsWindowsMR) ? -1.0f : 1.0f;
  const float triggerThreshold = gfxPrefs::VRControllerTriggerThreshold();

  for (uint32_t stateIndex = 0; stateIndex < kVRControllerMaxCount;
       stateIndex++) {
    ::vr::TrackedDeviceIndex_t trackedDevice =
      mControllerDeviceIndex[stateIndex];
    if (trackedDevice == 0) {
      continue;
    }
    VRControllerState& controllerState = aState.controllerState[stateIndex];
    const ::vr::ETrackedControllerRole role =
      mVRSystem->GetControllerRoleForTrackedDeviceIndex(trackedDevice);
    dom::GamepadHand hand = GetControllerHandFromControllerRole(role);
    controllerState.hand = hand;

    ::vr::VRControllerState_t vrControllerState;
    if (mVRSystem->GetControllerState(
          trackedDevice, &vrControllerState, sizeof(vrControllerState))) {
      uint32_t axisIdx = 0;
      uint32_t buttonIdx = 0;
      for (uint32_t j = 0; j < ::vr::k_unControllerStateAxisCount; ++j) {
        const uint32_t axisType = mVRSystem->GetInt32TrackedDeviceProperty(
          trackedDevice,
          static_cast<::vr::TrackedDeviceProperty>(::vr::Prop_Axis0Type_Int32 +
                                                   j));
        switch (axisType) {
          case ::vr::EVRControllerAxisType::k_eControllerAxis_Joystick:
          case ::vr::EVRControllerAxisType::k_eControllerAxis_TrackPad: {
            if (mIsWindowsMR) {
              // Adjust the input mapping for Windows MR which has
              // different order.
              axisIdx = (axisIdx == 0) ? 2 : 0;
              buttonIdx = (buttonIdx == 0) ? 4 : 0;
            }

            controllerState.axisValue[axisIdx] = vrControllerState.rAxis[j].x;
            ++axisIdx;
            controllerState.axisValue[axisIdx] =
              vrControllerState.rAxis[j].y * yAxisInvert;
            ++axisIdx;
            uint64_t buttonMask = ::vr::ButtonMaskFromId(
              static_cast<::vr::EVRButtonId>(::vr::k_EButton_Axis0 + j));

            UpdateButton(
              controllerState, vrControllerState, buttonIdx, buttonMask);
            ++buttonIdx;

            if (mIsWindowsMR) {
              axisIdx = (axisIdx == 4) ? 2 : 4;
              buttonIdx = (buttonIdx == 5) ? 1 : 2;
            }
            break;
          }
          case vr::EVRControllerAxisType::k_eControllerAxis_Trigger: {
            if (j <= 2) {
              UpdateTrigger(controllerState,
                            buttonIdx,
                            vrControllerState.rAxis[j].x,
                            triggerThreshold);
              ++buttonIdx;
            } else {
              // For SteamVR Knuckles.
              UpdateTrigger(controllerState,
                            buttonIdx,
                            vrControllerState.rAxis[j].x,
                            triggerThreshold);
              ++buttonIdx;
              UpdateTrigger(controllerState,
                            buttonIdx,
                            vrControllerState.rAxis[j].y,
                            triggerThreshold);
              ++buttonIdx;
            }
            break;
          }
        }
      }

      const uint64_t supportedButtons =
        mVRSystem->GetUint64TrackedDeviceProperty(
          trackedDevice, ::vr::Prop_SupportedButtons_Uint64);
      if (supportedButtons & BTN_MASK_FROM_ID(k_EButton_A)) {
        UpdateButton(controllerState,
                     vrControllerState,
                     buttonIdx,
                     BTN_MASK_FROM_ID(k_EButton_A));
        ++buttonIdx;
      }
      if (supportedButtons & BTN_MASK_FROM_ID(k_EButton_Grip)) {
        UpdateButton(controllerState,
                     vrControllerState,
                     buttonIdx,
                     BTN_MASK_FROM_ID(k_EButton_Grip));
        ++buttonIdx;
      }
      if (supportedButtons & BTN_MASK_FROM_ID(k_EButton_ApplicationMenu)) {
        UpdateButton(controllerState,
                     vrControllerState,
                     buttonIdx,
                     BTN_MASK_FROM_ID(k_EButton_ApplicationMenu));
        ++buttonIdx;
      }
      if (mIsWindowsMR) {
        // button 4 in Windows MR has already been assigned
        // to k_eControllerAxis_TrackPad.
        ++buttonIdx;
      }
      if (supportedButtons & BTN_MASK_FROM_ID(k_EButton_DPad_Left)) {
        UpdateButton(controllerState,
                     vrControllerState,
                     buttonIdx,
                     BTN_MASK_FROM_ID(k_EButton_DPad_Left));
        ++buttonIdx;
      }
      if (supportedButtons & BTN_MASK_FROM_ID(k_EButton_DPad_Up)) {
        UpdateButton(controllerState,
                     vrControllerState,
                     buttonIdx,
                     BTN_MASK_FROM_ID(k_EButton_DPad_Up));
        ++buttonIdx;
      }
      if (supportedButtons & BTN_MASK_FROM_ID(k_EButton_DPad_Right)) {
        UpdateButton(controllerState,
                     vrControllerState,
                     buttonIdx,
                     BTN_MASK_FROM_ID(k_EButton_DPad_Right));
        ++buttonIdx;
      }
      if (supportedButtons & BTN_MASK_FROM_ID(k_EButton_DPad_Down)) {
        UpdateButton(controllerState,
                     vrControllerState,
                     buttonIdx,
                     BTN_MASK_FROM_ID(k_EButton_DPad_Down));
        ++buttonIdx;
      }
    }
  }
}

void
OpenVRSession::UpdateControllerPoses(VRSystemState& aState)
{
  MOZ_ASSERT(mVRSystem);

  vr::InputPoseActionData_t poseData;

  for (uint32_t handIndex = 0; handIndex < EHand::Total; handIndex++) {
    VRControllerState& controllerState = aState.controllerState[handIndex];
    //  const ::vr::TrackedDevicePose_t& pose = poseData.pose;

    //  if (vr::VRInput()->GetPoseActionData(m_rHand[EHand::Right].m_actionPose,
    //  vr::TrackingUniverseStanding, 0, &poseData, sizeof( poseData ),
    //  vr::k_ulInvalidInputValueHandle ) != vr::VRInputError_None
    //      && poseData.bActive && poseData.pose.bPoseIsValid)
    if (vr::VRInput()->GetPoseActionData(m_rHand[handIndex].m_actionPose,
                                         vr::TrackingUniverseSeated,
                                         0,
                                         &poseData,
                                         sizeof(poseData),
                                         vr::k_ulInvalidInputValueHandle) !=
          vr::VRInputError_None ||
        !poseData.bActive || !poseData.pose.bPoseIsValid) {
      controllerState.isOrientationValid = false;
      controllerState.isPositionValid = false;
    } else {
      /* memcpy(&m_rHand[EHand::Right].m_rmat4Pose._11,
      &poseData.pose.mDeviceToAbsoluteTracking,
      sizeof(poseData.pose.mDeviceToAbsoluteTracking));
      m_rHand[EHand::Right].m_rmat4Pose.Transpose();
      m_rHand[EHand::Right].m_rmat4Pose.Invert();*/

      const ::vr::TrackedDevicePose_t& pose = poseData.pose;

      if (pose.bDeviceIsConnected) {
        controllerState.flags = (dom::GamepadCapabilityFlags::Cap_Orientation |
                                 dom::GamepadCapabilityFlags::Cap_Position);
      } else {
        controllerState.flags = dom::GamepadCapabilityFlags::Cap_None;
      }
      if (pose.bPoseIsValid &&
          pose.eTrackingResult == ::vr::TrackingResult_Running_OK) {
        gfx::Matrix4x4 m;

        // NOTE! mDeviceToAbsoluteTracking is a 3x4 matrix, not 4x4.  But
        // because of its arrangement, we can copy the 12 elements in and
        // then transpose them to the right place.  We do this so we can
        // pull out a Quaternion.
        memcpy(&m.components,
               &pose.mDeviceToAbsoluteTracking,
               sizeof(pose.mDeviceToAbsoluteTracking));
        m.Transpose();

        gfx::Quaternion rot;
        rot.SetFromRotationMatrix(m);
        rot.Invert();

        controllerState.pose.orientation[0] = rot.x;
        controllerState.pose.orientation[1] = rot.y;
        controllerState.pose.orientation[2] = rot.z;
        controllerState.pose.orientation[3] = rot.w;
        controllerState.pose.angularVelocity[0] = pose.vAngularVelocity.v[0];
        controllerState.pose.angularVelocity[1] = pose.vAngularVelocity.v[1];
        controllerState.pose.angularVelocity[2] = pose.vAngularVelocity.v[2];
        controllerState.pose.angularAcceleration[0] = 0.0f;
        controllerState.pose.angularAcceleration[1] = 0.0f;
        controllerState.pose.angularAcceleration[2] = 0.0f;
        controllerState.isOrientationValid = true;

        controllerState.pose.position[0] = m._41;
        controllerState.pose.position[1] = m._42;
        controllerState.pose.position[2] = m._43;
        controllerState.pose.linearVelocity[0] = pose.vVelocity.v[0];
        controllerState.pose.linearVelocity[1] = pose.vVelocity.v[1];
        controllerState.pose.linearVelocity[2] = pose.vVelocity.v[2];
        controllerState.pose.linearAcceleration[0] = 0.0f;
        controllerState.pose.linearAcceleration[1] = 0.0f;
        controllerState.pose.linearAcceleration[2] = 0.0f;
        controllerState.isPositionValid = true;
      }
    }
  }

  //    VRControllerState& controllerState =
  //    aState.controllerState[EHand::Left];
  //  //    const ::vr::TrackedDevicePose_t& pose = poseData.pose;
  //   if ( vr::VRInput()->GetPoseActionData( m_rHand[EHand::Left].m_actionPose,
  //   vr::TrackingUniverseSeated, 0, &poseData, sizeof( poseData ),
  //   vr::k_ulInvalidInputValueHandle ) != vr::VRInputError_None
  // 			|| !poseData.bActive || !poseData.pose.bPoseIsValid )
  // 		{
  //       controllerState.isOrientationValid = false;
  //       controllerState.isPositionValid = false;
  // 		//	m_rHand[eHand].m_bShowController = false;
  // 		}
  //     else {
  //     // memcpy(&m_rHand[EHand::Left].m_rmat4Pose._11,
  //     &poseData.pose.mDeviceToAbsoluteTracking,
  //     sizeof(poseData.pose.mDeviceToAbsoluteTracking));
  //     // m_rHand[EHand::Left].m_rmat4Pose.Transpose();
  //     // m_rHand[EHand::Left].m_rmat4Pose.Invert();
  //     const ::vr::TrackedDevicePose_t& pose = poseData.pose;
  //     if (pose.bDeviceIsConnected) {
  //       controllerState.flags = (dom::GamepadCapabilityFlags::Cap_Orientation
  //       |
  // 						       dom::GamepadCapabilityFlags::Cap_Position);
  //     } else {
  //       controllerState.flags = dom::GamepadCapabilityFlags::Cap_None;
  //     }
  //     if (pose.bPoseIsValid &&
  //         pose.eTrackingResult == ::vr::TrackingResult_Running_OK) {
  //       gfx::Matrix4x4 m;

  //       // NOTE! mDeviceToAbsoluteTracking is a 3x4 matrix, not 4x4.  But
  //       // because of its arrangement, we can copy the 12 elements in and
  //       // then transpose them to the right place.  We do this so we can
  //       // pull out a Quaternion.
  //       memcpy(&m.components, &pose.mDeviceToAbsoluteTracking,
  //       sizeof(pose.mDeviceToAbsoluteTracking)); m.Transpose();

  //       gfx::Quaternion rot;
  //       rot.SetFromRotationMatrix(m);
  //       rot.Invert();

  //       controllerState.pose.orientation[0] = rot.x;
  //       controllerState.pose.orientation[1] = rot.y;
  //       controllerState.pose.orientation[2] = rot.z;
  //       controllerState.pose.orientation[3] = rot.w;
  //       controllerState.pose.angularVelocity[0] = pose.vAngularVelocity.v[0];
  //       controllerState.pose.angularVelocity[1] = pose.vAngularVelocity.v[1];
  //       controllerState.pose.angularVelocity[2] = pose.vAngularVelocity.v[2];
  //       controllerState.pose.angularAcceleration[0] = 0.0f;
  //       controllerState.pose.angularAcceleration[1] = 0.0f;
  //       controllerState.pose.angularAcceleration[2] = 0.0f;
  //       controllerState.isOrientationValid = true;

  //       controllerState.pose.position[0] = m._41;
  //       controllerState.pose.position[1] = m._42;
  //       controllerState.pose.position[2] = m._43;
  //       controllerState.pose.linearVelocity[0] = pose.vVelocity.v[0];
  //       controllerState.pose.linearVelocity[1] = pose.vVelocity.v[1];
  //       controllerState.pose.linearVelocity[2] = pose.vVelocity.v[2];
  //       controllerState.pose.linearAcceleration[0] = 0.0f;
  //       controllerState.pose.linearAcceleration[1] = 0.0f;
  //       controllerState.pose.linearAcceleration[2] = 0.0f;
  //       controllerState.isPositionValid = true;
  //     }
  //   }

  //    controllerState = aState.controllerState[EHand::Right];
  //    //  const ::vr::TrackedDevicePose_t& pose = poseData.pose;

  //   //  if
  //   (vr::VRInput()->GetPoseActionData(m_rHand[EHand::Right].m_actionPose,
  //   vr::TrackingUniverseStanding, 0, &poseData, sizeof( poseData ),
  //   vr::k_ulInvalidInputValueHandle ) != vr::VRInputError_None
  //   //      && poseData.bActive && poseData.pose.bPoseIsValid)
  //    if ( vr::VRInput()->GetPoseActionData(
  //    m_rHand[EHand::Right].m_actionPose, vr::TrackingUniverseSeated, 0,
  //    &poseData, sizeof( poseData ), vr::k_ulInvalidInputValueHandle ) !=
  //    vr::VRInputError_None
  // 			|| !poseData.bActive || !poseData.pose.bPoseIsValid ) {
  //         controllerState.isOrientationValid = false;
  // 		    controllerState.isPositionValid = false;
  //       }
  //    else {
  // 		/* memcpy(&m_rHand[EHand::Right].m_rmat4Pose._11,
  // &poseData.pose.mDeviceToAbsoluteTracking,
  // sizeof(poseData.pose.mDeviceToAbsoluteTracking));
  // 		 m_rHand[EHand::Right].m_rmat4Pose.Transpose();
  // 		 m_rHand[EHand::Right].m_rmat4Pose.Invert();*/

  // 		const ::vr::TrackedDevicePose_t& pose = poseData.pose;

  // 		if (pose.bDeviceIsConnected) {
  // 		  controllerState.flags =
  // (dom::GamepadCapabilityFlags::Cap_Orientation |
  // 								   dom::GamepadCapabilityFlags::Cap_Position);
  // 		} else {
  // 		  controllerState.flags = dom::GamepadCapabilityFlags::Cap_None;
  // 		}
  // 		if (pose.bPoseIsValid &&
  // 			pose.eTrackingResult == ::vr::TrackingResult_Running_OK)
  // { 		  gfx::Matrix4x4 m;

  // 		  // NOTE! mDeviceToAbsoluteTracking is a 3x4 matrix, not 4x4.
  // But
  // 		  // because of its arrangement, we can copy the 12 elements in
  // and
  // 		  // then transpose them to the right place.  We do this so we
  // can
  // 		  // pull out a Quaternion.
  // 		  memcpy(&m.components,
  // 				 &pose.mDeviceToAbsoluteTracking,
  // 				 sizeof(pose.mDeviceToAbsoluteTracking));
  // 		  m.Transpose();

  // 		  gfx::Quaternion rot;
  // 		  rot.SetFromRotationMatrix(m);
  // 		  rot.Invert();

  // 		  controllerState.pose.orientation[0] = rot.x;
  // 		  controllerState.pose.orientation[1] = rot.y;
  // 		  controllerState.pose.orientation[2] = rot.z;
  // 		  controllerState.pose.orientation[3] = rot.w;
  // 		  controllerState.pose.angularVelocity[0] =
  // pose.vAngularVelocity.v[0]; 		  controllerState.pose.angularVelocity[1] =
  // pose.vAngularVelocity.v[1]; 		  controllerState.pose.angularVelocity[2] =
  // pose.vAngularVelocity.v[2]; 		  controllerState.pose.angularAcceleration[0] =
  // 0.0f; 		  controllerState.pose.angularAcceleration[1] = 0.0f;
  // 		  controllerState.pose.angularAcceleration[2] = 0.0f;
  // 		  controllerState.isOrientationValid = true;

  // 		  controllerState.pose.position[0] = m._41;
  // 		  controllerState.pose.position[1] = m._42;
  // 		  controllerState.pose.position[2] = m._43;
  // 		  controllerState.pose.linearVelocity[0] = pose.vVelocity.v[0];
  // 		  controllerState.pose.linearVelocity[1] = pose.vVelocity.v[1];
  // 		  controllerState.pose.linearVelocity[2] = pose.vVelocity.v[2];
  // 		  controllerState.pose.linearAcceleration[0] = 0.0f;
  // 		  controllerState.pose.linearAcceleration[1] = 0.0f;
  // 		  controllerState.pose.linearAcceleration[2] = 0.0f;
  // 		  controllerState.isPositionValid = true;
  // 		}
  //    }

  //   return;

  // ::vr::TrackedDevicePose_t poses[::vr::k_unMaxTrackedDeviceCount];
  // mVRSystem->GetDeviceToAbsoluteTrackingPose(::vr::TrackingUniverseSeated,
  // 0.0f,
  //                                            poses,
  //                                            ::vr::k_unMaxTrackedDeviceCount);

  // for (uint32_t stateIndex = 0; stateIndex < kVRControllerMaxCount;
  // stateIndex++) {
  //   ::vr::TrackedDeviceIndex_t trackedDevice =
  //   mControllerDeviceIndex[stateIndex]; if (trackedDevice == 0) {
  //     continue;
  //   }
  //   VRControllerState& controllerState = aState.controllerState[stateIndex];
  //   const ::vr::TrackedDevicePose_t& pose = poses[trackedDevice];

  //   if (pose.bDeviceIsConnected) {
  //     controllerState.flags = (dom::GamepadCapabilityFlags::Cap_Orientation |
  //                              dom::GamepadCapabilityFlags::Cap_Position);
  //   } else {
  //     controllerState.flags =  dom::GamepadCapabilityFlags::Cap_None;
  //   }
  //   if (pose.bPoseIsValid &&
  //       pose.eTrackingResult == ::vr::TrackingResult_Running_OK) {
  //     gfx::Matrix4x4 m;

  //     // NOTE! mDeviceToAbsoluteTracking is a 3x4 matrix, not 4x4.  But
  //     // because of its arrangement, we can copy the 12 elements in and
  //     // then transpose them to the right place.  We do this so we can
  //     // pull out a Quaternion.
  //     memcpy(&m.components, &pose.mDeviceToAbsoluteTracking,
  //     sizeof(pose.mDeviceToAbsoluteTracking)); m.Transpose();

  //     gfx::Quaternion rot;
  //     rot.SetFromRotationMatrix(m);
  //     rot.Invert();

  //     controllerState.pose.orientation[0] = rot.x;
  //     controllerState.pose.orientation[1] = rot.y;
  //     controllerState.pose.orientation[2] = rot.z;
  //     controllerState.pose.orientation[3] = rot.w;
  //     controllerState.pose.angularVelocity[0] = pose.vAngularVelocity.v[0];
  //     controllerState.pose.angularVelocity[1] = pose.vAngularVelocity.v[1];
  //     controllerState.pose.angularVelocity[2] = pose.vAngularVelocity.v[2];
  //     controllerState.pose.angularAcceleration[0] = 0.0f;
  //     controllerState.pose.angularAcceleration[1] = 0.0f;
  //     controllerState.pose.angularAcceleration[2] = 0.0f;
  //     controllerState.isOrientationValid = true;

  //     controllerState.pose.position[0] = m._41;
  //     controllerState.pose.position[1] = m._42;
  //     controllerState.pose.position[2] = m._43;
  //     controllerState.pose.linearVelocity[0] = pose.vVelocity.v[0];
  //     controllerState.pose.linearVelocity[1] = pose.vVelocity.v[1];
  //     controllerState.pose.linearVelocity[2] = pose.vVelocity.v[2];
  //     controllerState.pose.linearAcceleration[0] = 0.0f;
  //     controllerState.pose.linearAcceleration[1] = 0.0f;
  //     controllerState.pose.linearAcceleration[2] = 0.0f;
  //     controllerState.isPositionValid = true;
  //   } else {
  //     controllerState.isOrientationValid = false;
  //     controllerState.isPositionValid = false;
  //   }
  // }
}

void
OpenVRSession::UpdateControllerPosesObsolete(VRSystemState& aState)
{
  MOZ_ASSERT(mVRSystem);

  ::vr::TrackedDevicePose_t poses[::vr::k_unMaxTrackedDeviceCount];
  mVRSystem->GetDeviceToAbsoluteTrackingPose(
    ::vr::TrackingUniverseSeated, 0.0f, poses, ::vr::k_unMaxTrackedDeviceCount);

  for (uint32_t stateIndex = 0; stateIndex < kVRControllerMaxCount;
       stateIndex++) {
    ::vr::TrackedDeviceIndex_t trackedDevice =
      mControllerDeviceIndex[stateIndex];
    if (trackedDevice == 0) {
      continue;
    }
    VRControllerState& controllerState = aState.controllerState[stateIndex];
    const ::vr::TrackedDevicePose_t& pose = poses[trackedDevice];

    if (pose.bDeviceIsConnected) {
      controllerState.flags = (dom::GamepadCapabilityFlags::Cap_Orientation |
                               dom::GamepadCapabilityFlags::Cap_Position);
    } else {
      controllerState.flags = dom::GamepadCapabilityFlags::Cap_None;
    }
    if (pose.bPoseIsValid &&
        pose.eTrackingResult == ::vr::TrackingResult_Running_OK) {
      gfx::Matrix4x4 m;

      // NOTE! mDeviceToAbsoluteTracking is a 3x4 matrix, not 4x4.  But
      // because of its arrangement, we can copy the 12 elements in and
      // then transpose them to the right place.  We do this so we can
      // pull out a Quaternion.
      memcpy(&m.components,
             &pose.mDeviceToAbsoluteTracking,
             sizeof(pose.mDeviceToAbsoluteTracking));
      m.Transpose();

      gfx::Quaternion rot;
      rot.SetFromRotationMatrix(m);
      rot.Invert();

      controllerState.pose.orientation[0] = rot.x;
      controllerState.pose.orientation[1] = rot.y;
      controllerState.pose.orientation[2] = rot.z;
      controllerState.pose.orientation[3] = rot.w;
      controllerState.pose.angularVelocity[0] = pose.vAngularVelocity.v[0];
      controllerState.pose.angularVelocity[1] = pose.vAngularVelocity.v[1];
      controllerState.pose.angularVelocity[2] = pose.vAngularVelocity.v[2];
      controllerState.pose.angularAcceleration[0] = 0.0f;
      controllerState.pose.angularAcceleration[1] = 0.0f;
      controllerState.pose.angularAcceleration[2] = 0.0f;
      controllerState.isOrientationValid = true;

      controllerState.pose.position[0] = m._41;
      controllerState.pose.position[1] = m._42;
      controllerState.pose.position[2] = m._43;
      controllerState.pose.linearVelocity[0] = pose.vVelocity.v[0];
      controllerState.pose.linearVelocity[1] = pose.vVelocity.v[1];
      controllerState.pose.linearVelocity[2] = pose.vVelocity.v[2];
      controllerState.pose.linearAcceleration[0] = 0.0f;
      controllerState.pose.linearAcceleration[1] = 0.0f;
      controllerState.pose.linearAcceleration[2] = 0.0f;
      controllerState.isPositionValid = true;
    } else {
      controllerState.isOrientationValid = false;
      controllerState.isPositionValid = false;
    }
  }
}

void
OpenVRSession::GetControllerDeviceId(::vr::ETrackedDeviceClass aDeviceType,
                                     ::vr::TrackedDeviceIndex_t aDeviceIndex,
                                     nsCString& aId)
{
  switch (aDeviceType) {
    case ::vr::TrackedDeviceClass_Controller: {
      ::vr::ETrackedPropertyError err;
      uint32_t requiredBufferLen;
      bool isFound = false;
      char charBuf[128];
      requiredBufferLen = mVRSystem->GetStringTrackedDeviceProperty(
        aDeviceIndex, ::vr::Prop_RenderModelName_String, charBuf, 128, &err);
      if (requiredBufferLen > 128) {
        MOZ_CRASH("Larger than the buffer size.");
      }
      MOZ_ASSERT(requiredBufferLen && err == ::vr::TrackedProp_Success);
      nsCString deviceId(charBuf);
      if (deviceId.Find("knuckles") != kNotFound) {
        aId.AssignLiteral("OpenVR Knuckles");
        isFound = true;
      }
      requiredBufferLen = mVRSystem->GetStringTrackedDeviceProperty(
        aDeviceIndex, ::vr::Prop_SerialNumber_String, charBuf, 128, &err);
      if (requiredBufferLen > 128) {
        MOZ_CRASH("Larger than the buffer size.");
      }
      MOZ_ASSERT(requiredBufferLen && err == ::vr::TrackedProp_Success);
      deviceId.Assign(charBuf);
      if (deviceId.Find("MRSOURCE") != kNotFound) {
        aId.AssignLiteral("Spatial Controller (Spatial Interaction Source) ");
        mIsWindowsMR = true;
        isFound = true;
      }
      if (!isFound) {
        aId.AssignLiteral("OpenVR Gamepad");
      }
      break;
    }
    case ::vr::TrackedDeviceClass_GenericTracker: {
      aId.AssignLiteral("OpenVR Tracker");
      break;
    }
    default:
      MOZ_ASSERT(false);
      break;
  }
}

void
OpenVRSession::StartFrame(mozilla::gfx::VRSystemState& aSystemState)
{
  UpdateHeadsetPose(aSystemState);
  UpdateEyeParameters(aSystemState);

  if (gfxPrefs::VROpenVRActionInputEnabled()) {
    vr::VRActiveActionSet_t actionSet = {0};
    actionSet.ulActionSet = m_actionsetFirefox;
    vr::VRInput()->UpdateActionState(&actionSet, sizeof(actionSet), 1);
  }

  EnumerateControllers(aSystemState);
  if (gfxPrefs::VROpenVRActionInputEnabled()) {
    UpdateControllerButtons(aSystemState);
    UpdateControllerPoses(aSystemState);
  } else {
    UpdateControllerButtonsObsolete(aSystemState);
    UpdateControllerPosesObsolete(aSystemState);
  }

  UpdateTelemetry(aSystemState);
}

void
OpenVRSession::ProcessEvents(mozilla::gfx::VRSystemState& aSystemState)
{
  bool isHmdPresent = ::vr::VR_IsHmdPresent();
  if (!isHmdPresent) {
    mShouldQuit = true;
  }

  ::vr::VREvent_t event;
  while (mVRSystem && mVRSystem->PollNextEvent(&event, sizeof(event))) {
    switch (event.eventType) {
      case ::vr::VREvent_TrackedDeviceUserInteractionStarted:
        if (event.trackedDeviceIndex == ::vr::k_unTrackedDeviceIndex_Hmd) {
          aSystemState.displayState.mIsMounted = true;
        }
        break;
      case ::vr::VREvent_TrackedDeviceUserInteractionEnded:
        if (event.trackedDeviceIndex == ::vr::k_unTrackedDeviceIndex_Hmd) {
          aSystemState.displayState.mIsMounted = false;
        }
        break;
      case ::vr::EVREventType::VREvent_TrackedDeviceActivated:
        if (event.trackedDeviceIndex == ::vr::k_unTrackedDeviceIndex_Hmd) {
          aSystemState.displayState.mIsConnected = true;
        }
        break;
      case ::vr::EVREventType::VREvent_TrackedDeviceDeactivated:
        if (event.trackedDeviceIndex == ::vr::k_unTrackedDeviceIndex_Hmd) {
          aSystemState.displayState.mIsConnected = false;
        }
        break;
      case ::vr::EVREventType::VREvent_DriverRequestedQuit:
      case ::vr::EVREventType::VREvent_Quit:
      case ::vr::EVREventType::VREvent_ProcessQuit:
      case ::vr::EVREventType::VREvent_QuitAcknowledged:
      case ::vr::EVREventType::VREvent_QuitAborted_UserPrompt:
        mShouldQuit = true;
        break;
      default:
        // ignore
        break;
    }
  }
}

#if defined(XP_WIN)
bool
OpenVRSession::SubmitFrame(const mozilla::gfx::VRLayer_Stereo_Immersive& aLayer,
                           ID3D11Texture2D* aTexture)
{
  return SubmitFrame((void*)aTexture,
                     ::vr::ETextureType::TextureType_DirectX,
                     aLayer.mLeftEyeRect,
                     aLayer.mRightEyeRect);
}
#elif defined(XP_MACOSX)
bool
OpenVRSession::SubmitFrame(const mozilla::gfx::VRLayer_Stereo_Immersive& aLayer,
                           const VRLayerTextureHandle& aTexture)
{
  return SubmitFrame(aTexture,
                     ::vr::ETextureType::TextureType_IOSurface,
                     aLayer.mLeftEyeRect,
                     aLayer.mRightEyeRect);
}
#endif

bool
OpenVRSession::SubmitFrame(const VRLayerTextureHandle& aTextureHandle,
                           ::vr::ETextureType aTextureType,
                           const VRLayerEyeRect& aLeftEyeRect,
                           const VRLayerEyeRect& aRightEyeRect)
{
  ::vr::Texture_t tex;
#if defined(XP_MACOSX)
  // We get aTextureHandle from get_SurfaceDescriptorMacIOSurface() at
  // VRDisplayExternal. scaleFactor and opaque are skipped because they always
  // are 1.0 and false.
  RefPtr<MacIOSurface> surf = MacIOSurface::LookupSurface(aTextureHandle);
  if (!surf) {
    NS_WARNING("OpenVRSession::SubmitFrame failed to get a MacIOSurface");
    return false;
  }

  const void* ioSurface = surf->GetIOSurfacePtr();
  tex.handle = (void*)ioSurface;
#else
  tex.handle = aTextureHandle;
#endif
  tex.eType = aTextureType;
  tex.eColorSpace = ::vr::EColorSpace::ColorSpace_Auto;

  ::vr::VRTextureBounds_t bounds;
  bounds.uMin = aLeftEyeRect.x;
  bounds.vMin = 1.0 - aLeftEyeRect.y;
  bounds.uMax = aLeftEyeRect.x + aLeftEyeRect.width;
  bounds.vMax = 1.0 - (aLeftEyeRect.y + aLeftEyeRect.height);

  ::vr::EVRCompositorError err;
  err = mVRCompositor->Submit(::vr::EVREye::Eye_Left, &tex, &bounds);
  if (err != ::vr::EVRCompositorError::VRCompositorError_None) {
    printf_stderr("OpenVR Compositor Submit() failed.\n");
  }

  bounds.uMin = aRightEyeRect.x;
  bounds.vMin = 1.0 - aRightEyeRect.y;
  bounds.uMax = aRightEyeRect.x + aRightEyeRect.width;
  bounds.vMax = 1.0 - (aRightEyeRect.y + aRightEyeRect.height);

  err = mVRCompositor->Submit(::vr::EVREye::Eye_Right, &tex, &bounds);
  if (err != ::vr::EVRCompositorError::VRCompositorError_None) {
    printf_stderr("OpenVR Compositor Submit() failed.\n");
  }

  mVRCompositor->PostPresentHandoff();
  return true;
}

void
OpenVRSession::StopPresentation()
{
  mVRCompositor->ClearLastSubmittedFrame();

  ::vr::Compositor_CumulativeStats stats;
  mVRCompositor->GetCumulativeStats(&stats,
                                    sizeof(::vr::Compositor_CumulativeStats));
}

bool
OpenVRSession::StartPresentation()
{
  return true;
}

void
OpenVRSession::VibrateHaptic(uint32_t aControllerIdx,
                             uint32_t aHapticIndex,
                             float aIntensity,
                             float aDuration)
{
  MutexAutoLock lock(mControllerHapticStateMutex);
  if (aHapticIndex >= kNumOpenVRHaptics ||
      aControllerIdx >= kVRControllerMaxCount) {
    return;
  }

  ::vr::TrackedDeviceIndex_t deviceIndex =
    mControllerDeviceIndex[aControllerIdx];
  if (deviceIndex == 0) {
    return;
  }

  mHapticPulseRemaining[aControllerIdx][aHapticIndex] = aDuration;
  mHapticPulseIntensity[aControllerIdx][aHapticIndex] = aIntensity;

  /**
   *  TODO - The haptic feedback pulses will have latency of one frame and we
   *         are simulating intensity with pulse-width modulation.
   *         We should use of the OpenVR Input API to correct this
   *         and replace the TriggerHapticPulse calls which have been
   *         deprecated.
   */
}

void
OpenVRSession::StartHapticThread()
{
  if (!mHapticThread) {
    mHapticThread = new VRThread(NS_LITERAL_CSTRING("VR_OpenVR_Haptics"));
  }
  mHapticThread->Start();
}

void
OpenVRSession::StopHapticThread()
{
  if (mHapticThread) {
    mHapticThread->Shutdown();
    mHapticThread = nullptr;
  }
}

void
OpenVRSession::StartHapticTimer()
{
  if (!mHapticTimer && mHapticThread) {
    mLastHapticUpdate = TimeStamp();
    mHapticTimer = NS_NewTimer();
    mHapticTimer->SetTarget(mHapticThread->GetThread()->EventTarget());
    mHapticTimer->InitWithNamedFuncCallback(
      HapticTimerCallback,
      this,
      kVRHapticUpdateInterval,
      nsITimer::TYPE_REPEATING_PRECISE_CAN_SKIP,
      "OpenVRSession::HapticTimerCallback");
  }
}

void
OpenVRSession::StopHapticTimer()
{
  if (mHapticTimer) {
    mHapticTimer->Cancel();
    mHapticTimer = nullptr;
  }
}

/*static*/ void
OpenVRSession::HapticTimerCallback(nsITimer* aTimer, void* aClosure)
{
  /**
   * It is safe to use the pointer passed in aClosure to reference the
   * OpenVRSession object as the timer is canceled in OpenVRSession::Shutdown,
   * which is called by the OpenVRSession destructor, guaranteeing
   * that this function runs if and only if the VRManager object is valid.
   */
  OpenVRSession* self = static_cast<OpenVRSession*>(aClosure);

  if (gfxPrefs::VROpenVRActionInputEnabled()) {
    self->UpdateHaptics();
  } else {
    self->UpdateHapticsObsolete();
  }
}

void
OpenVRSession::UpdateHaptics()
{
  MOZ_ASSERT(mHapticThread->GetThread() == NS_GetCurrentThread());
  MOZ_ASSERT(mVRSystem);

  MutexAutoLock lock(mControllerHapticStateMutex);

  TimeStamp now = TimeStamp::Now();
  if (mLastHapticUpdate.IsNull()) {
    mLastHapticUpdate = now;
    return;
  }
  float deltaTime = (float)(now - mLastHapticUpdate).ToSeconds();
  mLastHapticUpdate = now;

  for (int iController = 0; iController < kVRControllerMaxCount;
       iController++) {
    for (int iHaptic = 0; iHaptic < kNumOpenVRHaptics; iHaptic++) {
      ::vr::TrackedDeviceIndex_t deviceIndex =
        mControllerDeviceIndex[iController];
      if (deviceIndex == 0) {
        continue;
      }
      float intensity = mHapticPulseIntensity[iController][iHaptic];
      float duration = mHapticPulseRemaining[iController][iHaptic];
      if (duration <= 0.0f || intensity <= 0.0f) {
        continue;
      }
      // We expect OpenVR to vibrate for 5 ms, but we found it only response the
      // commend ~ 3.9 ms. For duration time longer than 3.9 ms, we separate
      // them to a loop of 3.9 ms for make users feel that is a continuous
      // events.
      // const float microSec =
      //   (duration < 0.0039f ? duration : 0.0039f) * 1000000.0f * intensity;
      vr::VRInput()->TriggerHapticVibrationAction(m_rHand[iController].m_actionHaptic,
                                                  0, 1, 4.0f, 1.0f, vr::k_ulInvalidInputValueHandle);

      duration -= deltaTime;
      if (duration < 0.0f) {
        duration = 0.0f;
      }
      mHapticPulseRemaining[iController][iHaptic] = duration;
    }
  }
}

void
OpenVRSession::UpdateHapticsObsolete()
{
  MOZ_ASSERT(mHapticThread->GetThread() == NS_GetCurrentThread());
  MOZ_ASSERT(mVRSystem);

  MutexAutoLock lock(mControllerHapticStateMutex);

  TimeStamp now = TimeStamp::Now();
  if (mLastHapticUpdate.IsNull()) {
    mLastHapticUpdate = now;
    return;
  }
  float deltaTime = (float)(now - mLastHapticUpdate).ToSeconds();
  mLastHapticUpdate = now;

  for (int iController = 0; iController < kVRControllerMaxCount;
       iController++) {
    for (int iHaptic = 0; iHaptic < kNumOpenVRHaptics; iHaptic++) {
      ::vr::TrackedDeviceIndex_t deviceIndex =
        mControllerDeviceIndex[iController];
      if (deviceIndex == 0) {
        continue;
      }
      float intensity = mHapticPulseIntensity[iController][iHaptic];
      float duration = mHapticPulseRemaining[iController][iHaptic];
      if (duration <= 0.0f || intensity <= 0.0f) {
        continue;
      }
      // We expect OpenVR to vibrate for 5 ms, but we found it only response the
      // commend ~ 3.9 ms. For duration time longer than 3.9 ms, we separate
      // them to a loop of 3.9 ms for make users feel that is a continuous
      // events.
      const float microSec =
        (duration < 0.0039f ? duration : 0.0039f) * 1000000.0f * intensity;
      mVRSystem->TriggerHapticPulse(deviceIndex, iHaptic, (uint32_t)microSec);

      duration -= deltaTime;
      if (duration < 0.0f) {
        duration = 0.0f;
      }
      mHapticPulseRemaining[iController][iHaptic] = duration;
    }
  }
}

void
OpenVRSession::StopVibrateHaptic(uint32_t aControllerIdx)
{
  MutexAutoLock lock(mControllerHapticStateMutex);
  if (aControllerIdx >= kVRControllerMaxCount) {
    return;
  }
  for (int iHaptic = 0; iHaptic < kNumOpenVRHaptics; iHaptic++) {
    mHapticPulseRemaining[aControllerIdx][iHaptic] = 0.0f;
  }
}

void
OpenVRSession::StopAllHaptics()
{
  MutexAutoLock lock(mControllerHapticStateMutex);
  for (auto& controller : mHapticPulseRemaining) {
    for (auto& haptic : controller) {
      haptic = 0.0f;
    }
  }
}

void
OpenVRSession::UpdateTelemetry(VRSystemState& aSystemState)
{
  ::vr::Compositor_CumulativeStats stats;
  mVRCompositor->GetCumulativeStats(&stats,
                                    sizeof(::vr::Compositor_CumulativeStats));
  aSystemState.displayState.mDroppedFrameCount = stats.m_nNumReprojectedFrames;
}

} // namespace mozilla
} // namespace gfx
