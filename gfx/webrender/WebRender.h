#ifndef GFX_WEBRENDER_H
#define GFX_WEBRENDER_H

extern "C" {
	void* wr_create() {}; // Return wr_state
	void wr_init();
	void wr_render() {};
	void wr_terminate() {};
	void test_rust();
}

#endif // /* GFX_WEBRENDER_H */