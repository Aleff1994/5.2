#include "stdafx.h"
#include "ShaderProgram.h"

#include <vector>

ShaderProgram::ShaderProgram() : m_Id(0) {}
ShaderProgram::~ShaderProgram() { Reset(); }
ShaderProgram::ShaderProgram(ShaderProgram&& other) noexcept : m_Id(other.m_Id), m_UniformCache(std::move(other.m_UniformCache)) { other.m_Id = 0; }
ShaderProgram& ShaderProgram::operator=(ShaderProgram&& other) noexcept
{
	if (this != &other) { Reset(); m_Id = other.m_Id; m_UniformCache = std::move(other.m_UniformCache); other.m_Id = 0; }
	return *this;
}

GLuint ShaderProgram::Compile(GLenum type, const char* source, std::string& error)
{
	GLuint shader = glCreateShader(type);
	if (shader == 0) { error = "glCreateShader returned zero"; return 0; }
	glShaderSource(shader, 1, &source, NULL);
	glCompileShader(shader);
	GLint success = GL_FALSE;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
	if (success == GL_TRUE) return shader;
	GLint length = 0;
	glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
	std::vector<char> log(length > 1 ? length : 1, 0);
	glGetShaderInfoLog(shader, (GLsizei)log.size(), NULL, &log[0]);
	error.assign(&log[0]);
	glDeleteShader(shader);
	return 0;
}

bool ShaderProgram::Build(const char* vertexSource, const char* fragmentSource, std::string& error)
{
	Reset();
	GLuint vertex = Compile(GL_VERTEX_SHADER, vertexSource, error);
	if (vertex == 0) return false;
	GLuint fragment = Compile(GL_FRAGMENT_SHADER, fragmentSource, error);
	if (fragment == 0) { glDeleteShader(vertex); return false; }
	m_Id = glCreateProgram();
	if (m_Id != 0) { glAttachShader(m_Id, vertex); glAttachShader(m_Id, fragment); glLinkProgram(m_Id); }
	glDeleteShader(vertex);
	glDeleteShader(fragment);
	if (m_Id == 0) { error = "glCreateProgram returned zero"; return false; }
	GLint success = GL_FALSE;
	glGetProgramiv(m_Id, GL_LINK_STATUS, &success);
	if (success == GL_TRUE) return true;
	GLint length = 0;
	glGetProgramiv(m_Id, GL_INFO_LOG_LENGTH, &length);
	std::vector<char> log(length > 1 ? length : 1, 0);
	glGetProgramInfoLog(m_Id, (GLsizei)log.size(), NULL, &log[0]);
	error.assign(&log[0]);
	Reset();
	return false;
}

void ShaderProgram::Reset() { if (m_Id != 0) { glDeleteProgram(m_Id); m_Id = 0; } m_UniformCache.clear(); }
void ShaderProgram::Bind() const { glUseProgram(m_Id); }
GLint ShaderProgram::GetUniformLocation(const char* name)
{
	const std::string key(name != NULL ? name : "");
	auto found = m_UniformCache.find(key);
	if (found != m_UniformCache.end()) return found->second;
	const GLint location = glGetUniformLocation(m_Id, key.c_str());
	m_UniformCache.emplace(key, location);
	return location;
}
void ShaderProgram::SetMatrix4(const char* name, const float* value)
{
	const GLint location = GetUniformLocation(name);
	if (location >= 0 && value != NULL)
		glUniformMatrix4fv(location, 1, GL_FALSE, value);
}
void ShaderProgram::SetInt(const char* name, int value)
{
	const GLint location = GetUniformLocation(name);
	if (location >= 0) glUniform1i(location, value);
}
void ShaderProgram::SetFloat(const char* name, float value)
{
	const GLint location = GetUniformLocation(name);
	if (location >= 0) glUniform1f(location, value);
}
void ShaderProgram::SetVector4(const char* name, float x, float y, float z, float w)
{
	const GLint location = GetUniformLocation(name);
	if (location >= 0) glUniform4f(location, x, y, z, w);
}
GLuint ShaderProgram::GetId() const { return m_Id; }
size_t ShaderProgram::GetUniformCacheSize() const { return m_UniformCache.size(); }
