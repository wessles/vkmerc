#include "FlyCam.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/string_cast.hpp>
#include <iostream>

FlyCam::FlyCam() {}

FlyCam::FlyCam(GLFWwindow* window)
{
	this->window = window;
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	grabbed = true;
}

void FlyCam::setGrabbed(bool grabbed) {
	this->grabbed = grabbed;
	glfwSetInputMode(window, GLFW_CURSOR, grabbed ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
}

void FlyCam::update() {

	// movement

	bool w = glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS;
	bool a = glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS;
	bool s = glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS;
	bool d = glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS;
	bool space = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;
	bool shift = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS;
	bool ctrl = glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS;
	bool escape = glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS;

	if (escape && !wasPressingEscape)
		setGrabbed(!grabbed);
	wasPressingEscape = escape;

	glm::vec4 accel = { 0, 0, 0, 0 };
	float thrust = ctrl ? 0.01f : 0.001f;

	if (w) accel.z -= thrust;
	if (a) accel.x -= thrust;
	if (s) accel.z += thrust;
	if (d) accel.x += thrust;

	glm::mat4 transform = getTransform();
	// orient acceleration to transform
	accel = transform * accel;

	if (space) accel.y += thrust;
	if (shift) accel.y -= thrust;

	velocity += accel;
	velocity = glm::vec4(0.9f) * velocity;

	position += velocity;

	// rotation

	int width, height;
	glfwGetWindowSize(window, &width, &height);
	double centerX = width / 2, centerY = height / 2;

	double mouseX, mouseY;

	glfwGetCursorPos(window, &mouseX, &mouseY);
	if (grabbed) {
		lastMouseX = centerX;
		lastMouseY = centerY;
		glfwSetCursorPos(window, centerX, centerY);
	}

	double mouseDX = mouseX - lastMouseX;
	double mouseDY = mouseY - lastMouseY;

	lastMouseX = mouseX;
	lastMouseY = mouseY;

	float sensitivityX = 0.1;
	float sensitivityY = 0.1;

	rotation.y += -glm::radians(mouseDX * sensitivityX);
	rotation.x += -glm::radians(mouseDY * sensitivityY);
	double pio2 = 3.14 / 2.0;
	if (rotation.x > pio2) rotation.x = pio2;
	if (rotation.x < -pio2) rotation.x = -pio2;
}

glm::mat4 FlyCam::getTransform()
{
	glm::mat4 transform = glm::mat4(1.0f);
	transform = glm::translate(transform, static_cast<glm::vec3>(position));
	transform = glm::rotate(transform, rotation.y, glm::vec3(0, 1, 0));
	transform = glm::rotate(transform, rotation.x, glm::vec3(1, 0, 0));
	transform = glm::rotate(transform, rotation.z, glm::vec3(0, 0, 1));
	return transform;
}

glm::mat4 FlyCam::getProjMatrix(float width, float height, float zMin, float zMax)
{
	glm::mat4 proj = glm::perspective(glm::radians(90.0f), width / height, zMin, zMax);
	//proj = glm::ortho(-2.0f, 2.0f, -2.0f, 2.0f, -30.0f, 30.0f);
	proj[1][1] *= -1;
	return proj;
}