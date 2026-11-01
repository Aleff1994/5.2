#include "stdafx.h"
#include "GLResources.h"

GLBuffer::~GLBuffer() { Reset(); }
GLBuffer::GLBuffer(GLBuffer&& other) noexcept : m_Id(other.m_Id), m_Target(other.m_Target) { other.m_Id = 0; }
GLBuffer& GLBuffer::operator=(GLBuffer&& other) noexcept
{
	if (this != &other) { Reset(); m_Id = other.m_Id; m_Target = other.m_Target; other.m_Id = 0; }
	return *this;
}
bool GLBuffer::Create() { if (m_Id == 0) glGenBuffers(1, &m_Id); return m_Id != 0; }
void GLBuffer::Reset() { if (m_Id != 0) { glDeleteBuffers(1, &m_Id); m_Id = 0; } }
void GLBuffer::Bind() const { glBindBuffer(m_Target, m_Id); }
void GLBuffer::Upload(GLsizeiptr size, const void* data, GLenum usage) const { glBufferData(m_Target, size, data, usage); }
void GLBuffer::BindBase(GLuint index) const { glBindBufferBase(m_Target, index, m_Id); }
void GLBuffer::UploadSubData(GLintptr offset, GLsizeiptr size, const void* data) const { glBufferSubData(m_Target, offset, size, data); }
GLuint GLBuffer::GetId() const { return m_Id; }
GLenum GLBuffer::GetTarget() const { return m_Target; }

GLVertexArray::GLVertexArray() : m_Id(0) {}
GLVertexArray::~GLVertexArray() { Reset(); }
GLVertexArray::GLVertexArray(GLVertexArray&& other) noexcept : m_Id(other.m_Id) { other.m_Id = 0; }
GLVertexArray& GLVertexArray::operator=(GLVertexArray&& other) noexcept
{
	if (this != &other) { Reset(); m_Id = other.m_Id; other.m_Id = 0; }
	return *this;
}
bool GLVertexArray::Create() { if (m_Id == 0) glGenVertexArrays(1, &m_Id); return m_Id != 0; }
void GLVertexArray::Reset() { if (m_Id != 0) { glDeleteVertexArrays(1, &m_Id); m_Id = 0; } }
void GLVertexArray::Bind() const { glBindVertexArray(m_Id); }
GLuint GLVertexArray::GetId() const { return m_Id; }

GLTexture2D::GLTexture2D() : m_Id(0) {}
GLTexture2D::~GLTexture2D() { Reset(); }
GLTexture2D::GLTexture2D(GLTexture2D&& other) noexcept : m_Id(other.m_Id) { other.m_Id = 0; }
GLTexture2D& GLTexture2D::operator=(GLTexture2D&& other) noexcept
{
	if (this != &other) { Reset(); m_Id = other.m_Id; other.m_Id = 0; }
	return *this;
}
bool GLTexture2D::Create() { if (m_Id == 0) glGenTextures(1, &m_Id); return m_Id != 0; }
void GLTexture2D::Reset() { if (m_Id != 0) { glDeleteTextures(1, &m_Id); m_Id = 0; } }
void GLTexture2D::Bind() const { glBindTexture(GL_TEXTURE_2D, m_Id); }
GLuint GLTexture2D::GetId() const { return m_Id; }
