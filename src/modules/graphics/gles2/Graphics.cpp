/**
* Copyright (c) 2006-2012 LOVE Development Team
*
* This software is provided 'as-is', without any express or implied
* warranty.  In no event will the authors be held liable for any damages
* arising from the use of this software.
*
* Permission is granted to anyone to use this software for any purpose,
* including commercial applications, and to alter it and redistribute it
* freely, subject to the following restrictions:
*
* 1. The origin of this software must not be misrepresented; you must not
*    claim that you wrote the original software. If you use this software
*    in a product, an acknowledgment in the product documentation would be
*    appreciated but is not required.
* 2. Altered source versions must be plainly marked as such, and must not be
*    misrepresented as being the original software.
* 3. This notice may not be removed or altered from any source distribution.
**/

#include <common/config.h>
#include <common/math.h>
#include <common/Vector.h>

#include "Graphics.h"
#include "Context.h"
#include <window/ppapi/Window.h>

#include <vector>
#include <sstream>
#include <algorithm>
#include <iterator>

namespace love
{
namespace graphics
{
namespace gles2
{

	Graphics::Graphics()
		: currentFont(0), currentImageFilter(), lineStyle(LINE_SMOOTH), lineWidth(1), matrixLimit(0), userMatrices(0)
	{
		currentWindow = (love::window::ppapi::Window*)love::window::ppapi::Window::getSingleton();
	}

	Graphics::~Graphics()
	{
		if (currentFont != 0)
			currentFont->release();

		currentWindow->release();
	}

	const char * Graphics::getName() const
	{
		return "love.graphics.gles2";
	}

	bool Graphics::checkMode(int width, int height, bool fullscreen)
	{
		return currentWindow->checkWindowSize(width, height, fullscreen);
	}

	DisplayState Graphics::saveState()
	{
		DisplayState s;

		s.color = getColor();
		s.backgroundColor = getBackgroundColor();

		Context *ctx = getContext();

		s.blendMode = getBlendMode();
		//get line style
		s.lineStyle = lineStyle;
		//get the point size
		s.pointSize = ctx->getPointSize();
		//get point style
		s.pointStyle = Graphics::POINT_ROUGH;
		//get scissor status
		s.scissor = ctx->getCapability(GL_SCISSOR_TEST);
		//do we have scissor, if so, store the box
		if (s.scissor)
			glGetIntegerv(GL_SCISSOR_BOX, s.scissorBox);

		return s;
	}

	void Graphics::restoreState(const DisplayState & s)
	{
		setColor(s.color);
		setBackgroundColor(s.backgroundColor);
		setBlendMode(s.blendMode);
		setColorMode(s.colorMode);
		setLine(lineWidth, s.lineStyle);
		setPoint(s.pointSize, s.pointStyle);
		if (s.scissor)
			setScissor(s.scissorBox[0], s.scissorBox[1], s.scissorBox[2], s.scissorBox[3]);
		else
			setScissor();
	}

	bool Graphics::setMode(int width, int height, bool fullscreen, bool vsync, int fsaa)
	{
		// This operation destroys the OpenGL context, so
		// we must save the state.
		DisplayState tempState;
		if (isCreated())
			tempState = saveState();

		// Unload all volatile objects. These must be reloaded after
		// the display mode change.
		Volatile::unloadAll();

		bool success = currentWindow->setWindow(width, height, fullscreen, vsync, fsaa);

		// Regardless of failure, we'll have to set up OpenGL once again.
		width = currentWindow->getWidth();
		height = currentWindow->getHeight();

		// Okay, setup OpenGL.
		Context *ctx = resetContext();

		// Make sure antialiasing works when set elsewhere
//		ctx->setCapability(GL_MULTISAMPLE, true);

		// Enable blending
		ctx->setCapability(GL_BLEND, true);

		// "Normal" blending
		ctx->setBlendState(GL_FUNC_ADD, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		// Enable line/point smoothing.
		setLineStyle(LINE_SMOOTH);
		setPointStyle(POINT_SMOOTH);

		// Auto-generated mipmaps should be the best quality possible
		glHint(GL_GENERATE_MIPMAP_HINT, GL_NICEST);

		// Enable textures
		ctx->setCapability(GL_TEXTURE_2D, true);
		ctx->setActiveTextureUnit(0);

		ctx->setMainViewport(0, 0, width, height);

		// Set the projection matrix to an orthographic view with no depth
		ctx->projectionStack.push(Matrix());
		ctx->projectionStack.top() *= Matrix::ortho(0.0f, width, height, 0.0f);

		// Reset modelview matrix
		ctx->modelViewStack.top() = Matrix();

		// Set pixel row alignment
		glPixelStorei(GL_UNPACK_ALIGNMENT, 2);

		// Reload all volatile objects.
		if (!Volatile::loadAll())
			std::cerr << "Could not reload all volatile objects." << std::endl;

		// Restore the display state.
		restoreState(tempState);

		// Arbitrary matrix limit. Any larger is pretty pointless!
		matrixLimit = 60;

		return success;
	}

	void Graphics::getMode(int &width, int &height, bool &fullscreen, bool &vsync, int &fsaa)
	{
		currentWindow->getWindow(width, height, fullscreen, vsync, fsaa);
	}

	bool Graphics::toggleFullscreen()
	{
		int width, height, fsaa;
		bool fullscreen, vsync;
		currentWindow->getWindow(width, height, fullscreen, vsync, fsaa);
		return setMode(width, height, !fullscreen, vsync, fsaa);
	}


	void Graphics::reset()
	{
		DisplayState s;
		discardStencil();
		Canvas::bindDefaultCanvas();
		Shader::detach();
		restoreState(s);
	}

	void Graphics::clear()
	{
		glClear(GL_COLOR_BUFFER_BIT);
		getContext()->modelViewStack.top().setIdentity();
	}

	void Graphics::present()
	{
		currentWindow->swapBuffers();
	}

	void Graphics::setIcon(Image * image)
	{
		currentWindow->setIcon(image->getData());
	}

	void Graphics::setCaption(const char * caption)
	{
		std::string title(caption);
		currentWindow->setWindowTitle(title);
	}

	int Graphics::getCaption(lua_State * L)
	{
		std::string title = currentWindow->getWindowTitle();
		lua_pushstring(L, title.c_str());
		return 1;
	}

	int Graphics::getWidth()
	{
		return currentWindow->getWidth();
	}

	int Graphics::getHeight()
	{
		return currentWindow->getHeight();
	}

	int Graphics::getRenderWidth() {
		if (Canvas::current)
			return Canvas::current->getWidth();
		return currentWindow->getWidth();
	}

	int Graphics::getRenderHeight()
	{
		if (Canvas::current)
			return Canvas::current->getHeight();
		return currentWindow->getHeight();
	}

	bool Graphics::isCreated()
	{
		return currentWindow->isCreated();
	}

	int Graphics::getModes(lua_State * L)
	{
		int n;
		love::window::Window::WindowSize ** modes = currentWindow->getFullscreenSizes(n);

		if (modes == 0)
			return 0;

		lua_newtable(L);

		for (int i = 0; i < n ; i++)
		{
			lua_pushinteger(L, i+1);
			lua_newtable(L);

			// Inner table attribs.

			lua_pushstring(L, "width");
			lua_pushinteger(L, modes[i]->width);
			lua_settable(L, -3);

			lua_pushstring(L, "height");
			lua_pushinteger(L, modes[i]->height);
			lua_settable(L, -3);

			// Inner table attribs end.

			lua_settable(L, -3);

			delete modes[i];
		}

		delete[] modes;
		return 1;
	}

	void Graphics::setScissor(int x, int y, int width, int height)
	{
		const Context::Viewport& v = getContext()->getViewport();
		int newX = x + v.x;
		int newY = getRenderHeight() - (y + v.y + height);
		int newWidth = width;
		int newHeight = height;
		getContext()->setCapability(GL_SCISSOR_TEST, true);
		glScissor(newX, newY, newWidth, newHeight);
	}

	void Graphics::setScissor()
	{
		getContext()->setCapability(GL_SCISSOR_TEST, false);
	}

	int Graphics::getScissor(lua_State * L)
	{
		if (!getContext()->getCapability(GL_SCISSOR_TEST))
			return 0;

		GLint scissor[4];
		glGetIntegerv(GL_SCISSOR_BOX, scissor);

                const Context::Viewport& v = getContext()->getViewport();
		int x = scissor[0];
		int y = scissor[1];
		int width = scissor[2];
		int height = scissor[3];
		lua_pushnumber(L, x - v.x);
		lua_pushnumber(L, getRenderHeight() - (y + v.y + height));
		lua_pushnumber(L, width);
		lua_pushnumber(L, height);

		return 4;
	}

	void Graphics::defineStencil()
	{
		glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
		getContext()->setCapability(GL_STENCIL_TEST, true);
		glClear(GL_STENCIL_BUFFER_BIT);
		glStencilFunc(GL_ALWAYS, 1, 1);
		glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
	}

	void Graphics::useStencil(bool invert)
	{
		glStencilFunc(GL_EQUAL, (int)(!invert), 1); // invert ? 0 : 1
		glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	}

	void Graphics::discardStencil()
	{
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		getContext()->setCapability(GL_STENCIL_TEST, false);
	}

	Image * Graphics::newImage(love::image::ImageData * data)
	{
		// Create the image.
		Image * image = new Image(data);
		bool success;
		try
		{
			success = image->load();
		}
		catch (love::Exception & e)
		{
			image->release();
			throw love::Exception(e.what());
		}
		if (!success)
		{
			image->release();
			return 0;
		}

		image->setFilter(currentImageFilter);

		return image;
	}

	Quad * Graphics::newQuad(float x, float y, float w, float h, float sw, float sh)
	{
		Quad::Viewport v;
		v.x = x;
		v.y = y;
		v.w = w;
		v.h = h;
		return new Quad(v, sw, sh);
	}

	Font * Graphics::newFont(love::font::Rasterizer * r, const Image::Filter& filter)
	{
		Font * font = new Font(r, filter);

		// Load it and check for errors.
		if (!font)
		{
			delete font;
			return 0;
		}

		return font;
	}

	SpriteBatch * Graphics::newSpriteBatch(Image * image, int size, int usage)
	{
		SpriteBatch * t = NULL;
		try
		{
			t = new SpriteBatch(image, size, usage);
		}
		catch (love::Exception& e)
		{
			if (t) delete t;
			throw e;
		}
		return t;
	}

	ParticleSystem * Graphics::newParticleSystem(Image * image, int size)
	{
		return new ParticleSystem(image, size);
	}

	Canvas * Graphics::newCanvas(int width, int height)
	{
		Canvas * canvas = new Canvas(width, height);
		GLenum err = canvas->getStatus();

		// everything ok, reaturn canvas (early out)
		if (err == GL_FRAMEBUFFER_COMPLETE)
			return canvas;

		// create error message
		std::stringstream error_string;
		error_string << "Cannot create canvas: ";
		switch (err) {

			case GL_FRAMEBUFFER_UNSUPPORTED:
				error_string << "Not supported by your OpenGL implementation.";
				break;

			// remaining error codes are highly unlikely:
			case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
			case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
			case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS:
				error_string << "Error in implementation. Possible fix: Make canvas width and height powers of two.";
				break;

			default:
				// my intel hda card wrongly returns 0 to glCheckFramebufferStatus() but sets
				// no error flag. I think it meant to return GL_FRAMEBUFFER_UNSUPPORTED, but who
				// knows.
				if (glGetError() == GL_NO_ERROR)
					error_string << "May not be supported by your OpenGL implementation.";
				// the remaining error is an indication of a serious fuckup since it should
				// only be returned if glCheckFramebufferStatus() was called with the wrong
				// arguments.
				else
					error_string << "Cannot create canvas: Aliens did it (OpenGL error code: " << glGetError() << ")";
		}

		canvas->release();
		throw Exception(error_string.str().c_str());
		return NULL; // never reached
	}

	Shader *Graphics::newShader(const Shader::ShaderSources &sources)
	{
		return new Shader(sources);
	}

	void Graphics::setColor(const Color& c)
	{
		getContext()->setColor(c);
	}

	Color Graphics::getColor()
	{
		return getContext()->getColor();
	}

	void Graphics::setBackgroundColor(const Color& c)
	{
		// setClearColor affects the currently bound framebuffer, so we need to make
		// sure the default one is active.

		Canvas *curcanvas = Canvas::current;
		if (curcanvas)
			curcanvas->bindDefaultCanvas();

		getContext()->setClearColor(c);

		if (curcanvas)
			curcanvas->startGrab();
	}

	Color Graphics::getBackgroundColor()
	{
		return getContext()->getClearColor();
	}

	void Graphics::setFont( Font * font )
	{
		if (currentFont != 0)
			currentFont->release();

		currentFont = font;

		if (font != 0)
			currentFont->retain();
	}

	Font * Graphics::getFont()
	{
		return currentFont;
	}

	void Graphics::setBlendMode( Graphics::BlendMode mode )
	{
		Context::BlendState s;
		s.function = GL_FUNC_ADD;

		switch (mode)
		{
		case BLEND_ALPHA:
			s.src_rgb = GL_SRC_ALPHA;
			s.src_a = GL_ONE;
			s.dst_rgb = s.dst_a = GL_ONE_MINUS_SRC_ALPHA;
			break;
		case BLEND_MULTIPLICATIVE:
			s.src_rgb = s.src_a = GL_DST_COLOR;
			s.dst_rgb = s.dst_a = GL_ZERO;
			break;
		case BLEND_PREMULTIPLIED:
			s.src_rgb = s.src_a = GL_ONE;
			s.dst_rgb = s.dst_a = GL_ONE_MINUS_SRC_ALPHA;
			break;
		case BLEND_SUBTRACTIVE:
			s.function = GL_FUNC_REVERSE_SUBTRACT;
		case BLEND_ADDITIVE:
			s.src_rgb = s.src_a = GL_SRC_ALPHA;
			s.dst_rgb = s.dst_a = GL_ONE;
			break;
		case BLEND_NONE:
		default:
			s.src_rgb = s.src_a = GL_ONE;
			s.dst_rgb = s.dst_a = GL_ZERO;
			break;
	}

	getContext()->setBlendState(s);
	}

	void Graphics::setColorMode ( Graphics::ColorMode mode )
	{
#if 0
		if (mode == COLOR_MODULATE)
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		else if (mode == COLOR_COMBINE) {
			glTexEnvf(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_ADD_SIGNED);
			glTexEnvf(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
		}
		else // mode = COLOR_REPLACE
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
#endif
	}

	void Graphics::setDefaultImageFilter(const Image::Filter& f)
	{
		currentImageFilter = f;
	}

	Graphics::BlendMode Graphics::getBlendMode ()
	{
		Context::BlendState s = getContext()->getBlendState();

		if (s.function == GL_FUNC_REVERSE_SUBTRACT)
			return BLEND_SUBTRACTIVE;
		// Everything else has equation == GL_FUNC_ADD.
		else if (s.src_rgb == s.src_a && s.dst_rgb == s.dst_a)
		{
			if (s.src_rgb == GL_SRC_ALPHA && s.dst_rgb == GL_ONE)
				return BLEND_ADDITIVE;
			else if (s.src_rgb == GL_SRC_ALPHA && s.dst_rgb == GL_ONE_MINUS_SRC_ALPHA)
				return BLEND_ALPHA; // alpha blend mode fallback for very old OpenGL versions.
			else if (s.src_rgb == GL_DST_COLOR && s.dst_rgb == GL_ZERO)
				return BLEND_MULTIPLICATIVE;
			else if (s.src_rgb == GL_ONE && s.dst_rgb == GL_ONE_MINUS_SRC_ALPHA)
				return BLEND_PREMULTIPLIED;
			else if (s.src_rgb == GL_ONE && s.dst_rgb == GL_ZERO)
				return BLEND_NONE;
		}
		else if (s.src_rgb == GL_SRC_ALPHA && s.src_a == GL_ONE
				&& s.dst_rgb == GL_ONE_MINUS_SRC_ALPHA && s.dst_a == GL_ONE_MINUS_SRC_ALPHA)
			return BLEND_ALPHA;
		else
			throw love::Exception("Unknown blend mode.");

		return BLEND_MAX_ENUM; // Should never be reached.
	}

	Graphics::ColorMode Graphics::getColorMode()
	{
#if 0
		GLint mode;
		glGetTexEnviv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, &mode);

		if (mode == GL_MODULATE)
			return COLOR_MODULATE;
		else if (mode == GL_COMBINE)
			return COLOR_COMBINE;
		else // mode == GL_REPLACE
			return COLOR_REPLACE;
#else
                return COLOR_MAX_ENUM;
#endif
	}

	const Image::Filter& Graphics::getDefaultImageFilter() const
	{
		return currentImageFilter;
	}

	void Graphics::setLineWidth( float width )
	{
		lineWidth = width;
	}

	void Graphics::setLineStyle(Graphics::LineStyle style)
	{
		lineStyle = style;
	}

	void Graphics::setLine( float width, Graphics::LineStyle style )
	{
		setLineWidth(width);

		if (style == 0)
			return;
		setLineStyle(style);
	}

	float Graphics::getLineWidth()
	{
		float w;
		glGetFloatv(GL_LINE_WIDTH, &w);
		return w;
	}

	Graphics::LineStyle Graphics::getLineStyle()
	{
		return lineStyle;
	}

	void Graphics::setPointSize( float size )
	{
		getContext()->setPointSize(size);
	}

	void Graphics::setPointStyle( Graphics::PointStyle style )
	{
	}

	void Graphics::setPoint( float size, Graphics::PointStyle style )
	{
		getContext()->setPointSize(size);
	}

	float Graphics::getPointSize()
	{
		return getContext()->getPointSize();
	}

	Graphics::PointStyle Graphics::getPointStyle()
	{
		return POINT_ROUGH;
	}

	int Graphics::getMaxPointSize()
	{
		return getContext()->getMaxPointSize();
	}

	void Graphics::print( const char * str, float x, float y , float angle, float sx, float sy, float ox, float oy, float kx, float ky)
	{
		if (currentFont != 0)
		{
			std::string text(str);
			currentFont->print(text, x, y, angle, sx, sy, ox, oy, kx, ky);
		}
	}

	void Graphics::printf( const char * str, float x, float y, float wrap, AlignMode align)
	{
		if (currentFont == 0)
			return;

		using namespace std;
		string text(str);
		vector<string> lines_to_draw = currentFont->getWrap(text, wrap);

		// now for the actual printing
		vector<string>::const_iterator line_iter, line_end = lines_to_draw.end();
		for (line_iter = lines_to_draw.begin(); line_iter != line_end; ++line_iter)
		{
			float width = static_cast<float>(currentFont->getWidth( *line_iter ));
			switch (align) {
				case ALIGN_RIGHT:
					currentFont->print(*line_iter, ceil(x + wrap - width), ceil(y));
					break;
				case ALIGN_CENTER:
					currentFont->print(*line_iter, ceil(x + (wrap - width) / 2), ceil(y));
					break;
				case ALIGN_LEFT:
				default:
					currentFont->print(*line_iter, ceil(x), ceil(y));
					break;
			}
			y += currentFont->getHeight() * currentFont->getLineHeight();
		}
	}

	/**
	* Primitives
	**/

	void Graphics::point( float x, float y )
	{
		float coord[] = {x, y};

		Context *ctx = getContext();

		ctx->bindTexture(0);
		ctx->useVertexAttribArrays(Context::ATTRIB_VERTEX);
		ctx->vertexAttribPointer(Context::ATTRIB_VERTEX, 2, GL_FLOAT, 0, coord);

		ctx->setupRender();
		glDrawArrays(GL_POINTS, 0, 1);
	}

	// Calculate line boundary points u1 and u2. Sketch:
	//              u1
	// -------------+---...___
	//              |         ```'''--  ---
	// p- - - - - - q- - . _ _           | w/2
	//              |          ` ' ' r   +
	// -------------+---...___           | w/2
	//              u2         ```'''-- ---
	//
	// u1 and u2 depend on four things:
	//   - the half line width w/2
	//   - the previous line vertex p
	//   - the current line vertex q
	//   - the next line vertex r
	//
	// u1/u2 are the intersection points of the parallel lines to p-q and q-r,
	// i.e. the point where
	//
	//    (p + w/2 * n1) + mu * (q - p) = (q + w/2 * n2) + lambda * (r - q)   (u1)
	//    (p - w/2 * n1) + mu * (q - p) = (q - w/2 * n2) + lambda * (r - q)   (u2)
	//
	// with n1,n2 being the normals on the segments p-q and q-r:
	//
	//    n1 = perp(q - p) / |q - p|
	//    n2 = perp(r - q) / |r - q|
	//
	// The intersection points can be calculated using cramers rule.
	static void pushIntersectionPoints(Vector *vertices, Vector* overdraw,
			int pos, int count, float hw, float inv_hw,
			const Vector& p, const Vector& q, const Vector& r)
	{
		// calculate line directions
		Vector s = (q - p);
		Vector t = (r - q);

		// calculate vertex displacement vectors
		Vector n1 = s.getNormal();
		Vector n2 = t.getNormal();
		n1.normalize();
		n2.normalize();
		float det_norm = n1 ^ n2; // will be close to zero if the angle between the normals is sharp
		n1 *= hw;
		n2 *= hw;

		// lines parallel -> assume intersection at displacement points
		if (fabs(det_norm) <= .03)
		{
			vertices[pos]   = q - n2;
			vertices[pos+1] = q + n2;
		}
		// real intersection -> calculate boundary intersection points with cramers rule
		else
		{
			float det = s ^ t;
			Vector d = n1 - n2;
			Vector b = s - d; // s = q - p
			Vector c = s + d;
			float lambda = (b ^ t) / det;
			float mu     = (c ^ t) / det;

			// ordering for GL_TRIANGLE_STRIP
			vertices[pos]   = p + s*mu - n1;     // u1
			vertices[pos+1] = p + s*lambda + n1; // u2
		}

		if (overdraw)
		{
			// displacement of the overdraw vertices (works by magic).
			Vector x = (vertices[pos] - q) * inv_hw;

			overdraw[pos]   = vertices[pos];
			overdraw[pos+1] = vertices[pos] + x;

			overdraw[2*count-pos-2] = vertices[pos+1];
			overdraw[2*count-pos-1] = vertices[pos+1] - x;
		}
	}

	// precondition:
	// context->setVertexAttribArray(ATTRIB_VERTEX, true);
	static void draw_overdraw(Vector* overdraw, size_t count, float pixel_size, bool looping)
	{
		// if not looping, the outer overdraw vertices need to be displaced
		// to cover the line endings, i.e.:
		// +- - - - //- - +         +- - - - - //- - - +
		// +-------//-----+         : +-------//-----+ :
		// | core // line |   -->   : | core // line | :
		// +-----//-------+         : +-----//-------+ :
		// +- - //- - - - +         +- - - //- - - - - +
		if (!looping)
		{
			Vector s = overdraw[1] - overdraw[3];
			s.normalize();
			s *= pixel_size;
			overdraw[1] += s;
			overdraw[2*count-1] += s;

			Vector t = overdraw[count-1] - overdraw[count-3];
			t.normalize();
			t *= pixel_size;
			overdraw[count-1] += t;
			overdraw[count+1] += t;

			// we need to draw two more triangles to close the
			// overdraw at the line start.
			overdraw[2*count]   = overdraw[0];
			overdraw[2*count+1] = overdraw[1];
		}

		Context *ctx = getContext();

		// prepare colors:
		// even indices in overdraw* point to inner vertices => alpha = current-alpha,
		// odd indices point to outer vertices => alpha = 0.
		const Color &c = ctx->getColor();

		Color *colors = new Color[2*count+2];
		for (size_t i = 0; i < 2*count+2; ++i)
		{
			colors[i] = Color(c.r,
							  c.g,
							  c.b,
							  // avoids branching. equiv to if (i%2 == 1) colors[i].a = 0;
							  c.a * GLubyte(i%2 == 0));
		}

		// draw faded out line halos
		ctx->setVertexAttribArray(Context::ATTRIB_COLOR, true);

		ctx->vertexAttribPointer(Context::ATTRIB_COLOR, 4, GL_UNSIGNED_BYTE, 0, colors);
		ctx->vertexAttribPointer(Context::ATTRIB_VERTEX, 2, GL_FLOAT, 0, (const GLvoid *)overdraw);

		ctx->setupRender();
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 2*count + 2 * int(!looping));

		// "if GL_COLOR_ARRAY is enabled, the value of the current color is
		// undefined after glDrawArrays executes"
		ctx->setColor(ctx->getColor());

		delete[] colors;
	}

	void Graphics::polyline(const float* coords, size_t count)
	{
		Context *ctx = getContext();

		Vector *vertices = new Vector[count]; // two vertices for every line end-point
		Vector *overdraw = NULL;

		Vector p,q,r;
		bool looping = (coords[0] == coords[count-2]) && (coords[1] == coords[count-1]);

		float halfwidth       = lineWidth/2.f;
		float pixel_size      = 1.f;
		float overdraw_factor = .0f;

		if (lineStyle == LINE_SMOOTH)
		{
			overdraw = new Vector[2*count+2];
			// TODO: is there a better way to get the pixel size at the current scale?
			const float *m = ctx->modelViewStack.top().getElements();
			float det  = m[0]*m[5]*m[10] + m[4]*m[9]*m[2] + m[8]*m[1]*m[6];
			det       -= m[2]*m[5]*m[8]  + m[6]*m[9]*m[0] + m[10]*m[1]*m[4];
			pixel_size = 1.f / sqrtf(det);

			overdraw_factor = pixel_size / halfwidth;
			halfwidth = std::max(.0f, halfwidth - .25f*pixel_size);
		}

		// get line vertex boundaries
		// if not looping, extend the line at the beginning, else use last point as `p'
		r = Vector(coords[0], coords[1]);
		if (!looping)
			q = r * 2 - Vector(coords[2], coords[3]);
		else
			q = Vector(coords[count-4], coords[count-3]);

		for (size_t i = 0; i+3 < count; i += 2)
		{
			p = q;
			q = r;
			r = Vector(coords[i+2], coords[i+3]);
			pushIntersectionPoints(vertices, overdraw, i, count, halfwidth, overdraw_factor, p,q,r);
		}

		// if not looping, extend the line at the end, else use first point as `r'
		p = q;
		q = r;
		if (!looping)
			r += q - p;
		else
			r = Vector(coords[2], coords[3]);
		pushIntersectionPoints(vertices, overdraw, count-2, count, halfwidth, overdraw_factor, p,q,r);
		// end get line vertex boundaries

		// draw the core line
		ctx->bindTexture(0);
		ctx->useVertexAttribArrays(Context::ATTRIB_VERTEX);
		ctx->vertexAttribPointer(Context::ATTRIB_VERTEX, 2, GL_FLOAT, 0, (const GLvoid *)vertices);

		ctx->setupRender();
		glDrawArrays(GL_TRIANGLE_STRIP, 0, count);

		// draw the line halo (antialiasing)
		if (lineStyle == LINE_SMOOTH)
			draw_overdraw(overdraw, count, pixel_size, looping);

		// cleanup
		delete[] vertices;
		if (lineStyle == LINE_SMOOTH)
			delete[] overdraw;
	}

	void Graphics::triangle(DrawMode mode, float x1, float y1, float x2, float y2, float x3, float y3 )
	{
		float coords[] = { x1,y1, x2,y2, x3,y3, x1,y1 };
		polygon(mode, coords, 4 * 2);
	}

	void Graphics::rectangle(DrawMode mode, float x, float y, float w, float h)
	{
		quad(mode, x,y, x,y+h, x+w,y+h, x+w,y);
	}

	void Graphics::quad(DrawMode mode, float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4 )
	{
		float coords[] = { x1,y1, x2,y2, x3,y3, x4,y4, x1,y1 };
		polygon(mode, coords, 5 * 2);
	}

	void Graphics::circle(DrawMode mode, float x, float y, float radius, int points)
	{
		float two_pi = static_cast<float>(LOVE_M_PI * 2);
		if (points <= 0) points = 1;
		float angle_shift = (two_pi / points);
		float phi = .0f;

		float *coords = new float[2 * (points + 1)];
		for (int i = 0; i < points; ++i, phi += angle_shift)
		{
			coords[2*i]   = x + radius * cos(phi);
			coords[2*i+1] = y + radius * sin(phi);
		}

		coords[2*points]   = coords[0];
		coords[2*points+1] = coords[1];

		polygon(mode, coords, (points + 1) * 2);

		delete[] coords;
	}

	void Graphics::arc(DrawMode mode, float x, float y, float radius, float angle1, float angle2, int points)
	{
		// Nothing to display with no points or equal angles. (Or is there with line mode?)
		if (points <= 0 || angle1 == angle2)
			return;

		// Oh, you want to draw a circle?
		if (fabs(angle1 - angle2) >= 2.0f * (float) LOVE_M_PI)
		{
			circle(mode, x, y, radius, points);
			return;
		}

		float angle_shift = (angle2 - angle1) / points;
		// Bail on precision issues.
		if (angle_shift == 0.0)
			return;

		float phi = angle1;
		int num_coords = (points + 3) * 2;
		float *coords = new float[num_coords];
		coords[0] = coords[num_coords - 2] = x;
		coords[1] = coords[num_coords - 1] = y;

		for (int i = 0; i <= points; ++i, phi += angle_shift)
		{
			coords[2 * (i+1)]     = x + radius * cos(phi);
			coords[2 * (i+1) + 1] = y + radius * sin(phi);
		}

		// GL_POLYGON can only fill-draw convex polygons, so we need to do stuff manually here
		if (mode == DRAW_LINE)
		{
			polyline(coords, num_coords); // Artifacts at sharp angles if set to looping.
		}
		else
		{
			Context *ctx = getContext();
			ctx->bindTexture(0);
			ctx->useVertexAttribArrays(Context::ATTRIB_VERTEX);
			ctx->vertexAttribPointer(Context::ATTRIB_VERTEX, 2, GL_FLOAT, 0, (const GLvoid *) coords);

			ctx->setupRender();
			glDrawArrays(GL_TRIANGLE_FAN, 0, points + 2);
		}

		delete[] coords;
	}

	/// @param mode    the draw mode
	/// @param coords  the coordinate array
	/// @param count   the number of coordinates/size of the array
	void Graphics::polygon(DrawMode mode, const float* coords, size_t count)
	{
		// coords is an array of a closed loop of vertices, i.e.
		// coords[count-2] = coords[0], coords[count-1] = coords[1]
		if (mode == DRAW_LINE)
		{
			polyline(coords, count);
		}
		else
		{
			Context *ctx = getContext();
			ctx->bindTexture(0);
			ctx->useVertexAttribArrays(Context::ATTRIB_VERTEX);
			ctx->vertexAttribPointer(Context::ATTRIB_VERTEX, 2, GL_FLOAT, 0, (const GLvoid *) coords);

			ctx->setupRender();
			glDrawArrays(GL_TRIANGLE_FAN, 0, count/2-1); // opengl will close the polygon for us
		}
	}

	love::image::ImageData * Graphics::newScreenshot(love::image::Image * image)
	{
		int w = getWidth();
		int h = getHeight();

		int row = 4*w;

		int size = row*h;

		GLubyte * pixels = new GLubyte[size];
		GLubyte * screenshot = new GLubyte[size];

		glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

		// OpenGL sucks and reads pixels from the lower-left. Let's fix that.

		GLubyte *src = pixels - row, *dst = screenshot + size;

		for (int i = 0; i < h; ++i)
		{
			memcpy(dst-=row, src+=row, row);
		}

		love::image::ImageData * img = image->newImageData(w, h, (void*)screenshot);

		delete [] pixels;
		delete [] screenshot;

		return img;
	}

	void Graphics::push()
	{
		Context *ctx = getContext();

		if (ctx->modelViewStack.size() == matrixLimit)
			throw Exception("Maximum stack depth reached. (More pushes than pops?)");

		ctx->modelViewStack.push(ctx->modelViewStack.top());
	}

	void Graphics::pop()
	{
		Context *ctx = getContext();

		if (ctx->modelViewStack.size() <= 1)
			throw Exception("Minimum stack depth reached. (More pops than pushes?)");

		ctx->modelViewStack.pop();
	}

	void Graphics::rotate(float r)
	{
		getContext()->modelViewStack.top().rotate(r);
	}

	void Graphics::scale(float x, float y)
	{
		getContext()->modelViewStack.top().scale(x, y);
	}

	void Graphics::translate(float x, float y)
	{
		getContext()->modelViewStack.top().translate(x, y);
	}

	void Graphics::shear(float kx, float ky)
	{
		getContext()->modelViewStack.top().shear(kx, ky);
	}

	void Graphics::drawTest(Image * image, float x, float y, float a, float sx, float sy, float ox, float oy)
	{
          // TODO(binji): implement
#if 0
		image->bind();

		// Buffer for transforming the image.
		vertex buf[4];

		Matrix t;
		t.translate(x, y);
		t.rotate(a);
		t.scale(sx, sy);
		t.translate(ox, oy);
		t.transform(buf, image->getVertices(), 4);

		const vertex * vertices = image->getVertices();

		glEnableClientState(GL_VERTEX_ARRAY);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glVertexPointer(2, GL_FLOAT, sizeof(vertex), (GLvoid*)&buf[0].x);
		glTexCoordPointer(2, GL_FLOAT, sizeof(vertex), (GLvoid*)&vertices[0].s);
		glDrawArrays(GL_QUADS, 0, 4);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		glDisableClientState(GL_VERTEX_ARRAY);
#endif
	}

	bool Graphics::hasFocus()
	{
		return currentWindow->hasFocus();
	}
} // gles2
} // graphics
} // love
