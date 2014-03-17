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

#include "freeCamera.h"
#include "GLFW/glfw3.h"
#include "logger.h"

namespace framework
{

FreeCamera::FreeCamera():
	m_moveForward(false),
	m_moveBackward(false),
	m_moveLeft(false),
	m_moveRight(false),
	m_rotationMode(false),
	m_speed(50.0f)
{

}

void FreeCamera::initWithPositionDirection(int width, int height, const vector3& from, const vector3& to)
{
	init(width, height);
	setPosition(from);
	
	vector3 dir = to - from;
	dir.norm();
	quaternion q;
	q.set_from_axes(vector3(0, 0, 1), dir);
	setOrientation(q);
	matrix33 m(q);
	vector3 angles = m.to_euler();

	m_angles.x = n_rad2deg(angles.y);
	m_angles.y = n_rad2deg(angles.x);
}

void FreeCamera::onKeyButton(int key, int scancode, int action, int mods)
{
	if (key == GLFW_KEY_W || key == GLFW_KEY_UP)
	{
		if (action == GLFW_PRESS) m_moveForward = true;
		else if (action == GLFW_RELEASE) m_moveForward = false;
	}
	else if (key == GLFW_KEY_S || key == GLFW_KEY_DOWN)
	{
		if (action == GLFW_PRESS) m_moveBackward = true;
		else if (action == GLFW_RELEASE) m_moveBackward = false;
	}
	else if (key == GLFW_KEY_A || key == GLFW_KEY_LEFT)
	{
		if (action == GLFW_PRESS) m_moveLeft = true;
		else if (action == GLFW_RELEASE) m_moveLeft = false;
	}
	else if (key == GLFW_KEY_D || key == GLFW_KEY_RIGHT)
	{
		if (action == GLFW_PRESS) m_moveRight = true;
		else if (action == GLFW_RELEASE) m_moveRight = false;
	}
}
	
void FreeCamera::onMouseButton(double xpos, double ypos, int button, int action, int mods)
{
	if (button == GLFW_MOUSE_BUTTON_LEFT)
	{
		if (action == GLFW_PRESS) 
		{ 
			m_rotationMode = true;
			m_lastMousePosition.x = (float)xpos;
			m_lastMousePosition.y = (float)ypos;
			m_currentMousePosition = m_lastMousePosition;
		}
		else if (action == GLFW_RELEASE) m_rotationMode = false;
	}
}
	
void FreeCamera::onMouseMove(double xpos, double ypos)
{
	if (m_rotationMode)
	{
		m_currentMousePosition = vector2((float)xpos, (float)ypos);
	}
}

void FreeCamera::update(double elapsedTime)
{
	if (m_moveForward)
	{
		m_position += (m_orientation.z_direction() * m_speed * (float)elapsedTime);
	}

	if (m_moveBackward)
	{
		m_position -= (m_orientation.z_direction() * m_speed * (float)elapsedTime);
	}

	if (m_moveLeft)
	{
		m_position += (m_orientation.x_direction() * m_speed * (float)elapsedTime);
	}

	if (m_moveRight)
	{
		m_position -= (m_orientation.x_direction() * m_speed * (float)elapsedTime);
	}

	if (m_rotationMode)
	{
		vector2 delta = m_currentMousePosition - m_lastMousePosition;
		vector2 old_angles = m_angles;
		m_angles.x -= delta.x * 200.0f * (float)elapsedTime;
		m_angles.y += delta.y * 200.0f * (float)elapsedTime;
		if (m_angles.y > 89.9f) m_angles.y = 89.9f;
		if (m_angles.y < -89.9) m_angles.y = -89.9f;
		//utils::Logger::toLogWithFormat("angles = (%.2f; %.2f)\n", m_angles.x, m_angles.y);

		if (!old_angles.isequal(m_angles, 0.001f))
		{
			quaternion q1;
			q1.set_rotate_axis_angle(vector3(0, 1, 0), n_deg2rad(m_angles.x));
			quaternion q2;
			q2.set_rotate_axis_angle(vector3(1, 0, 0), n_deg2rad(m_angles.y));
			m_orientation = q1 * q2;
		}

		m_lastMousePosition = m_currentMousePosition;
	}
}

}