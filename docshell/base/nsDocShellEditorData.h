/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsDocShellEditorData_h__
#define nsDocShellEditorData_h__

#ifndef nsCOMPtr_h___
#include "nsCOMPtr.h"
#endif

#include "mozilla/HTMLEditor.h"
#include "mozilla/RefPtr.h"
#include "nsIHTMLDocument.h"

class nsIDocShell;
class nsIEditingSession;

class nsDocShellEditorData {
 public:
  explicit nsDocShellEditorData(nsIDocShell* aOwningDocShell);
  ~nsDocShellEditorData();

  nsresult MakeEditable(bool aWaitForUriLoad);
  bool GetEditable();
  nsresult CreateEditor();
  nsresult GetEditingSession(nsIEditingSession** aResult);
  mozilla::HTMLEditor* GetHTMLEditor() const { return mHTMLEditor; }
  nsresult SetHTMLEditor(mozilla::HTMLEditor* aHTMLEditor);
  void TearDownEditor();
  nsresult DetachFromWindow();
  nsresult ReattachToWindow(nsIDocShell* aDocShell);
  bool WaitingForLoad() const { return mMakeEditable; }

 protected:
  void EnsureEditingSession();

  // The doc shell that owns us. Weak ref, since it always outlives us.
  nsIDocShell* mDocShell;

  // Only present for the content root docShell. Session is owned here.
  nsCOMPtr<nsIEditingSession> mEditingSession;

  // If this frame is editable, store HTML editor here. It's owned here.
  RefPtr<mozilla::HTMLEditor> mHTMLEditor;

  // Backup for the corresponding nsIHTMLDocument's  editing state while
  // the editor is detached.
  nsIHTMLDocument::EditingState mDetachedEditingState;

  // Indicates whether to make an editor after a url load.
  bool mMakeEditable;

  // Denotes if the editor is detached from its window. The editor is detached
  // while it's stored in the session history bfcache.
  bool mIsDetached;

  // Backup for mMakeEditable while the editor is detached.
  bool mDetachedMakeEditable;
};

#endif  // nsDocShellEditorData_h__
