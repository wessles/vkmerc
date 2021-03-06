#include "FlyCam.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/string_cast.hpp>
#include <iostream>

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

	bool minus = glfwGetKey(window, GLFW_KEY_I) == GLFW_PRESS;
	bool plus = glfwGetKey(window, GLFW_KEY_O) == GLFW_PRESS;

	if (escape && !wasPressingEscape)
		setGrabbed(!grabbed);
	wasPressingEscape = escape;

	glm::vec4 accel = { 0, 0, 0, 0 };
	float thrust = ctrl ? 0.01f : 0.005f;

	if (w) accel.z -= thrust;
	if (a) accel.x -= thrust;
	if (s) accel.z += thrust;
	if (d) accel.x += thrust;

	if (minus) {
		FOV += 0.5f;
	}
	if (plus) {
		FOV -= 0.5f;
	}

	glm::mat4 transform = getTransform();
	// orient acceleration to transform
	accel = transform * accel;
	// project WASD movement onto the xz plane
	accel.y = 0;

	if (space) accel.y += 0.005f;
	if (shift) accel.y -= 0.005f;

	velocity += accel;
	velocity = glm::vec4(0.9f) * velocity;

	position += velocity;

	// rotation
	if (!this->grabbed) {
		return;
	}
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

float FlyCam::getFOV() {
	return FOV;
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

glm::mat4 FlyCam::getProjMatrix(float width, float height, float zMin, float zMax, float FOV)
{
	if (FOV < 0)
		FOV = this->FOV;
	glm::mat4 proj = glm::perspective(glm::radians(FOV), width / height, zMin, zMax);
	proj[1][1] *= -1;
	return proj;
}