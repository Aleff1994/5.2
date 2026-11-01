#include "stdafx.h"
#include "RenderMatrices.h"

#include <glm/gtc/matrix_transform.hpp>

RenderMatrices::RenderMatrices()
	: m_Model(1.0f), m_View(1.0f), m_Projection(1.0f), m_MVP(1.0f)
{
}

void RenderMatrices::SetModel(const glm::mat4& model) { m_Model = model; RecalculateMVP(); }
void RenderMatrices::SetView(const glm::mat4& view) { m_View = view; RecalculateMVP(); }
void RenderMatrices::SetProjection(const glm::mat4& projection) { m_Projection = projection; RecalculateMVP(); }

void RenderMatrices::SetPerspective(float fovDegrees, float aspect, float nearPlane, float farPlane)
{
	m_Projection = glm::perspective(glm::radians(fovDegrees), aspect, nearPlane, farPlane);
	RecalculateMVP();
}

void RenderMatrices::SetLookAt(const glm::vec3& eye, const glm::vec3& center, const glm::vec3& up)
{
	m_View = glm::lookAt(eye, center, up);
	RecalculateMVP();
}

void RenderMatrices::SetOrthographic(float left, float right, float bottom, float top,
	float nearPlane, float farPlane)
{
	m_Projection = glm::ortho(left, right, bottom, top, nearPlane, farPlane);
	RecalculateMVP();
}

const glm::mat4& RenderMatrices::GetModel() const { return m_Model; }
const glm::mat4& RenderMatrices::GetView() const { return m_View; }
const glm::mat4& RenderMatrices::GetProjection() const { return m_Projection; }
const glm::mat4& RenderMatrices::GetMVP() const { return m_MVP; }

void RenderMatrices::RecalculateMVP()
{
	m_MVP = m_Projection * m_View * m_Model;
}
