#include "OrbitCam.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/string_cast.hpp>
#include <iostream>

OrbitCam::OrbitCam(GLFWwindow* window)
{
	this->window = window;
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	grabbed = true;
}

void OrbitCam::setGrabbed(bool grabbed) {
	this->grabbed = grabbed;
	glfwSetInputMode(window, GLFW_CURSOR, grabbed ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
}

void OrbitCam::update() {

	// movement

	bool w = glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS;
	bool a = glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS;
	bool s = glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS;
	bool d = glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS;

	bool escape = glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS;
	bool minus = glfwGetKey(window, GLFW_KEY_I) == GLFW_PRESS;
	bool plus = glfwGetKey(window, GLFW_KEY_O) == GLFW_PRESS;

	if (escape && !wasPressingEscape)
		setGrabbed(!grabbed);
	wasPressingEscape = escape;

	if (w) distance -= 0.1f;
	if (s) distance += 0.1f;

	if (minus) {
		FOV += 0.5f;
	}
	if (plus) {
		FOV -= 0.5f;
	}

	// rotation
	if (!grabbed) {
		rotation.y += glm::radians(0.5f);
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

	rotation.y += glm::radians(mouseDX * sensitivityX);
	rotation.x += glm::radians(mouseDY * sensitivityY);
	const double pio2 = 3.14 / 2.0;
	if (rotation.x > pio2) rotation.x = pio2;
	if (rotation.x < -pio2) rotation.x = -pio2;
}

glm::mat4 OrbitCam::getTransform()
{
	glm::mat4 rotmat = glm::mat4(1.0f);
	rotmat = glm::rotate(rotmat, rotation.y, glm::vec3(0, 1, 0));
	rotmat = glm::rotate(rotmat, rotation.x, glm::vec3(1, 0, 0));

	glm::vec4 translate = rotmat * glm::vec4(0, 0, distance, 0.0f);
	glm::vec3 cam = glm::vec3(translate);
	glm::vec3 or = glm::vec3(0);

	return glm::translate(glm::mat4(1.0f), cam) * rotmat;
}

glm::mat4 OrbitCam::getProjMatrix(float width, float height, float zMin, float zMax, float FOV)
{
	if (FOV < 0)
		FOV = this->FOV;
	glm::mat4 proj = glm::perspective(glm::radians(FOV), width / height, zMin, zMax);
	proj[1][1] *= -1;
	return proj;
}