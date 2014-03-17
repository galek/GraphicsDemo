/*
 * Copyright (c) 2014 Roman Kuznetsov 
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "lightManager.h"
#include "line3D.h"
#include "gpuprogram.h"
#include "standardGpuPrograms.h"

namespace framework
{

void LightManager::addLightSource(const LightSource& lightSource)
{
	LightData data;
	data.lightSource = lightSource;
	data.lineDebugVis.reset(new Line3D());
	if (data.lightSource.type == LightType::DirectLight)
	{
		createDirectLightDebugVisualization(data.lineDebugVis);
	}
	else if (data.lightSource.type == LightType::OmniLight)
	{
		createOmniLightDebugVisualization(data.lineDebugVis);
	}
	else if (data.lightSource.type == LightType::SpotLight)
	{
		createSpotLightDebugVisualization(data.lineDebugVis);
	}
	m_lightSources.push_back(data);
}

void LightManager::renderDebugVisualization(const matrix44& viewProjection)
{
	for (size_t i = 0; i < m_lightSources.size(); i++)
	{
		if (m_lightSources[i].lightSource.type == LightType::OmniLight)
		{
			matrix44 model;
			model.set_translation(m_lightSources[i].lightSource.position);
			m_lightSources[i].lineDebugVis->renderWithStandardGpuProgram(model * viewProjection, m_lightSources[i].lightSource.diffuseColor, false);
		}
		else
		{
			matrix44 model(m_lightSources[i].lightSource.orientation);
			model.set_translation(m_lightSources[i].lightSource.position);
			m_lightSources[i].lineDebugVis->renderWithStandardGpuProgram(model * viewProjection, m_lightSources[i].lightSource.diffuseColor, true);

			auto program = framework::StandardGpuPrograms::getArrowRenderer();
			if (program->use())
			{
				program->setVector<StandardUniforms>(STD_UF::ORIENTATION, m_lightSources[i].lightSource.orientation);
				program->setVector<StandardUniforms>(STD_UF::POSITION, m_lightSources[i].lightSource.position);
				program->setMatrix<StandardUniforms>(STD_UF::MODELVIEWPROJECTION_MATRIX, viewProjection);
				program->setVector<StandardUniforms>(STD_UF::COLOR, vector4(m_lightSources[i].lightSource.diffuseColor));

				glDrawArrays(GL_POINTS, 0, 1);
			}
		}
	}
}

void LightManager::createDirectLightDebugVisualization(const std::shared_ptr<Line3D>& line)
{
	std::vector<vector3> points;
	points.reserve(4);
	points.push_back(vector3(-3, 3, 0));
	points.push_back(vector3(3, 3, 0));
	points.push_back(vector3(3, -3, 0));
	points.push_back(vector3(-3, -3, 0));
	line->initWithArray(points);
}

void LightManager::createOmniLightDebugVisualization(const std::shared_ptr<Line3D>& line)
{
	std::vector<vector3> points;
	points.reserve(43);
	for (int i = 0; i <= 12; i++)
	{
		float x = n_cos(2.0f * N_PI * i / 12.0f);
		float y = n_sin(2.0f * N_PI * i / 12.0f);
		points.push_back(vector3(x, y, 0));
	}
	for (int i = 0; i <= 12; i++)
	{
		float x = n_cos(2.0f * N_PI * i / 12.0f);
		float z = n_sin(2.0f * N_PI * i / 12.0f);
		points.push_back(vector3(x, 0, z));
	}
	for (int i = 0; i <= 3; i++)
	{
		float x = n_cos(2.0f * N_PI * i / 12.0f);
		float z = n_sin(2.0f * N_PI * i / 12.0f);
		points.push_back(vector3(x, 0, z));
	}
	for (int i = 0; i <= 12; i++)
	{
		float z = n_cos(2.0f * N_PI * i / 12.0f);
		float y = n_sin(2.0f * N_PI * i / 12.0f);
		points.push_back(vector3(0, y, z));
	}
	line->initWithArray(points);
}

void LightManager::createSpotLightDebugVisualization(const std::shared_ptr<Line3D>& line)
{
	std::vector<vector3> points;
	points.reserve(12);
	for (int i = 0; i < 12; i++)
	{
		float x = n_cos(2.0f * N_PI * i / 12.0f);
		float y = n_sin(2.0f * N_PI * i / 12.0f);
		points.push_back(vector3(x, y, 0));
	}
	line->initWithArray(points);
}

LightRawData LightManager::getRawLightData(size_t index)
{
	LightRawData result;
	if (index >= m_lightSources.size())
	{
		result.positionOrDirection[0] = result.positionOrDirection[1] = result.positionOrDirection[2] = 0;
		result.lightType = (unsigned int)LightType::OmniLight;
		result.diffuseColor[0] = result.diffuseColor[1] = result.diffuseColor[2] = 1.0f;
		result.falloff = 1000.0f;
		result.ambientColor[0] = result.ambientColor[1] = result.ambientColor[2] = 0.3f;
		result.angle = 60.0f;
		result.specularColor[0] = result.specularColor[1] = result.specularColor[2] = 1.0f;
		
		return result;
	}

	const LightSource& source = getLightSource(index);
	vector3 dir = source.orientation.z_direction();
	result.positionOrDirection[0] = source.type == LightType::DirectLight ? dir.x : source.position.x;
	result.positionOrDirection[1] = source.type == LightType::DirectLight ? dir.y : source.position.y;
	result.positionOrDirection[2] = source.type == LightType::DirectLight ? dir.z : source.position.z;
	result.lightType = (unsigned int)source.type;
	result.diffuseColor[0] = source.diffuseColor.x;
	result.diffuseColor[1] = source.diffuseColor.y;
	result.diffuseColor[2] = source.diffuseColor.z;
	result.falloff = source.falloff;
	result.ambientColor[0] = source.ambientColor.x;
	result.ambientColor[1] = source.ambientColor.y;
	result.ambientColor[2] = source.ambientColor.z;
	result.angle = source.angle;
	result.specularColor[0] = source.specularColor.x;
	result.specularColor[1] = source.specularColor.y;
	result.specularColor[2] = source.specularColor.z;

	return result;
}

}