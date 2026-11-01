#pragma once

#include <string>
#include <unordered_map>

class ShaderProgram
{
public:
	ShaderProgram();
	~ShaderProgram();
	ShaderProgram(ShaderProgram&& other) noexcept;
	ShaderProgram& operator=(ShaderProgram&& other) noexcept;
	bool Build(const char* vertexSource, const char* fragmentSource, std::string& error);
	void Reset();
	void Bind() const;
	GLint GetUniformLocation(const char* name);
	void SetInt(const char* name, int value);
	void SetFloat(const char* name, float value);
	void SetVector4(const char* name, float x, float y, float z, float w);
	void SetMatrix4(const char* name, const float* value);
	GLuint GetId() const;
	size_t GetUniformCacheSize() const;

private:
	ShaderProgram(const ShaderProgram&) = delete;
	ShaderProgram& operator=(const ShaderProgram&) = delete;
	GLuint Compile(GLenum type, const char* source, std::string& error);
	GLuint m_Id;
	std::unordered_map<std::string, GLint> m_UniformCache;
};
