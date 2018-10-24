#ifndef __RAS_OPENGL_PIXEL_BUFFER_H__
#define __RAS_OPENGL_PIXEL_BUFFER_H__

#include <memory>

#include "GPU_glew.h"

class RAS_OpenGLPixelBuffer
{
private:
	GLuint m_pbo;
	// Memory size of current buffer.
	unsigned int m_size;

public:
	RAS_OpenGLPixelBuffer();
	~RAS_OpenGLPixelBuffer();

	void Copy(int x, int y, int width, int height);
	unsigned int *Map();
	void Unmap();
};

#endif  // __RAS_OPENGL_PIXEL_BUFFER_H__
