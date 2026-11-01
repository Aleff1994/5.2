#pragma once

class GLBuffer
{
public:
	GLBuffer() : m_Id(0), m_Target(0) {}
	explicit GLBuffer(GLenum target) : m_Id(0), m_Target(target) {}
	~GLBuffer();
	GLBuffer(GLBuffer&& other) noexcept;
	GLBuffer& operator=(GLBuffer&& other) noexcept;
	bool Create();
	void Reset();
	void Bind() const;
	void BindBase(GLuint index) const;
	void Upload(GLsizeiptr size, const void* data, GLenum usage) const;
	void UploadSubData(GLintptr offset, GLsizeiptr size, const void* data) const;
	GLuint GetId() const;
	GLenum GetTarget() const;

private:
	GLBuffer(const GLBuffer&) = delete;
	GLBuffer& operator=(const GLBuffer&) = delete;
	GLuint m_Id;
	GLenum m_Target;
};

class GLVertexArray
{
public:
	GLVertexArray();
	~GLVertexArray();
	GLVertexArray(GLVertexArray&& other) noexcept;
	GLVertexArray& operator=(GLVertexArray&& other) noexcept;
	bool Create();
	void Reset();
	void Bind() const;
	GLuint GetId() const;

private:
	GLVertexArray(const GLVertexArray&) = delete;
	GLVertexArray& operator=(const GLVertexArray&) = delete;
	GLuint m_Id;
};

class GLTexture2D
{
public:
	GLTexture2D();
	~GLTexture2D();
	GLTexture2D(GLTexture2D&& other) noexcept;
	GLTexture2D& operator=(GLTexture2D&& other) noexcept;
	bool Create();
	void Reset();
	void Bind() const;
	GLuint GetId() const;

private:
	GLTexture2D(const GLTexture2D&) = delete;
	GLTexture2D& operator=(const GLTexture2D&) = delete;
	GLuint m_Id;
};
