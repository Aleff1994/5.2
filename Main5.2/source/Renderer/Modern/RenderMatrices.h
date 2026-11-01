#pragma once

#include <glm/glm.hpp>

class RenderMatrices
{
public:
	RenderMatrices();
	void SetModel(const glm::mat4& model);
	void SetView(const glm::mat4& view);
	void SetProjection(const glm::mat4& projection);
	void SetPerspective(float fovDegrees, float aspect, float nearPlane, float farPlane);
	void SetLookAt(const glm::vec3& eye, const glm::vec3& center, const glm::vec3& up);
	void SetOrthographic(float left, float right, float bottom, float top,
		float nearPlane, float farPlane);
	const glm::mat4& GetModel() const;
	const glm::mat4& GetView() const;
	const glm::mat4& GetProjection() const;
	const glm::mat4& GetMVP() const;

private:
	void RecalculateMVP();
	glm::mat4 m_Model;
	glm::mat4 m_View;
	glm::mat4 m_Projection;
	glm::mat4 m_MVP;
};
