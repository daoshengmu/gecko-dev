#ifndef GFX_WEBRENDER_H
#define GFX_WEBRENDER_H

namespace mozilla {
	namespace gfx {
		struct Webrender{
			void* glContext;
			void* wrState;

			Webrender()
				:glContext(nullptr), wrState(nullptr) {}
		};
	}
}

// extern GLContext* gGLContext;

extern mozilla::gfx::Webrender* gWebrender;

extern "C" {
	void* wr_create(); // Return wr_state
	// void wr_init();
	// void wr_render();
	// void wr_terminate();
	void wr_dp_begin(void* wrState);
	void wr_dp_end(void* wrState);
	void wr_dp_push_rect(void* wrState, float x, float y,
											 float w, float h,
											 float r, float g, float b, float a);
	void test_rust();
}

#endif // /* GFX_WEBRENDER_H */