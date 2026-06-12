#ifndef MINI_GL_H
#define MINI_GL_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
#ifndef APIENTRY
#define APIENTRY
#endif
#ifndef APIENTRYP
#define APIENTRYP APIENTRY *
#endif
typedef unsigned int GLenum; typedef unsigned char GLboolean; typedef unsigned int GLbitfield; typedef void GLvoid; typedef signed char GLbyte; typedef short GLshort; typedef int GLint; typedef int GLsizei; typedef unsigned char GLubyte; typedef unsigned short GLushort; typedef unsigned int GLuint; typedef float GLfloat; typedef double GLdouble; typedef char GLchar;
#define GL_FALSE 0
#define GL_TRUE 1
#define GL_POINTS 0x0000
#define GL_QUADS 0x0007
#define GL_DEPTH_BUFFER_BIT 0x00000100
#define GL_COLOR_BUFFER_BIT 0x00004000
#define GL_BLEND 0x0BE2
#define GL_CULL_FACE 0x0B44
#define GL_DEPTH_TEST 0x0B71
#define GL_PACK_ALIGNMENT 0x0D05
#define GL_MODELVIEW 0x1700
#define GL_PROJECTION 0x1701
#define GL_TEXTURE_2D 0x0DE1
#define GL_UNSIGNED_BYTE 0x1401
#define GL_FLOAT 0x1406
#define GL_RGB 0x1907
#define GL_RGBA 0x1908
#define GL_SRC_ALPHA 0x0302
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#define GL_ONE 1
#define GL_RENDERER 0x1F01
#define GL_VERSION 0x1F02
#define GL_LINEAR 0x2601
#define GL_LINEAR_MIPMAP_LINEAR 0x2703
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_REPEAT 0x2901
#define GL_VERTEX_ARRAY 0x8074
#define GL_NORMAL_ARRAY 0x8075
#define GL_TEXTURE_COORD_ARRAY 0x8078
#define GL_GENERATE_MIPMAP 0x8191
#define GL_TEXTURE0 0x84C0
#define GL_TEXTURE1 0x84C1
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER 0x8B31
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82

typedef GLuint (APIENTRYP PFNGLCREATESHADERPROC)(GLenum type);
typedef void (APIENTRYP PFNGLSHADERSOURCEPROC)(GLuint shader, GLsizei count, const GLchar **string, const GLint *length);
typedef void (APIENTRYP PFNGLCOMPILESHADERPROC)(GLuint shader);
typedef void (APIENTRYP PFNGLGETSHADERIVPROC)(GLuint shader, GLenum pname, GLint *params);
typedef void (APIENTRYP PFNGLGETSHADERINFOLOGPROC)(GLuint shader, GLsizei maxLength, GLsizei *length, GLchar *infoLog);
typedef GLuint (APIENTRYP PFNGLCREATEPROGRAMPROC)(void);
typedef void (APIENTRYP PFNGLATTACHSHADERPROC)(GLuint program, GLuint shader);
typedef void (APIENTRYP PFNGLLINKPROGRAMPROC)(GLuint program);
typedef void (APIENTRYP PFNGLGETPROGRAMIVPROC)(GLuint program, GLenum pname, GLint *params);
typedef void (APIENTRYP PFNGLGETPROGRAMINFOLOGPROC)(GLuint program, GLsizei maxLength, GLsizei *length, GLchar *infoLog);
typedef void (APIENTRYP PFNGLUSEPROGRAMPROC)(GLuint program);
typedef GLint (APIENTRYP PFNGLGETUNIFORMLOCATIONPROC)(GLuint program, const GLchar *name);
typedef void (APIENTRYP PFNGLUNIFORM1FPROC)(GLint location, GLfloat v0);
typedef void (APIENTRYP PFNGLUNIFORM1IPROC)(GLint location, GLint v0);
typedef void (APIENTRYP PFNGLUNIFORM3FPROC)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
typedef void (APIENTRYP PFNGLUNIFORM3FVPROC)(GLint location, GLsizei count, const GLfloat *value);
typedef void (APIENTRYP PFNGLUNIFORM4FVPROC)(GLint location, GLsizei count, const GLfloat *value);
typedef void (APIENTRYP PFNGLUNIFORMMATRIX3FVPROC)(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (APIENTRYP PFNGLACTIVETEXTUREPROC)(GLenum texture);

void glBegin(GLenum mode); void glEnd(void); void glVertex2f(GLfloat x, GLfloat y); void glVertex3f(GLfloat x, GLfloat y, GLfloat z); void glNormal3f(GLfloat x, GLfloat y, GLfloat z); void glTexCoord2f(GLfloat s, GLfloat t); void glColor3f(GLfloat r, GLfloat g, GLfloat b); void glColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a);
void glGenTextures(GLsizei n, GLuint *textures); void glBindTexture(GLenum target, GLuint texture); void glTexParameteri(GLenum target, GLenum pname, GLint param); void glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
void glEnable(GLenum cap); void glDisable(GLenum cap); void glBlendFunc(GLenum sfactor, GLenum dfactor); void glDepthMask(GLboolean flag); void glClear(GLbitfield mask); void glClearColor(GLfloat r, GLfloat g, GLfloat b, GLfloat a);
void glMatrixMode(GLenum mode); void glLoadIdentity(void); void glFrustum(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar); void glOrtho(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar); void glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z); void glTranslatef(GLfloat x, GLfloat y, GLfloat z); void glPushMatrix(void); void glPopMatrix(void);
void glEnableClientState(GLenum array); void glDisableClientState(GLenum array); void glVertexPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer); void glNormalPointer(GLenum type, GLsizei stride, const GLvoid *pointer); void glTexCoordPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer); void glDrawArrays(GLenum mode, GLint first, GLsizei count);
void glPixelStorei(GLenum pname, GLint param); void glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels); void glPointSize(GLfloat size); const GLubyte *glGetString(GLenum name);
#ifdef __cplusplus
}
#endif
#endif
