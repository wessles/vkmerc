#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>

namespace vku {
	glm::mat4 fitLightProjMatToCameraFrustum(glm::mat4 frustumMat, glm::vec4 lightDirection, float dim, glm::mat4 sceneAABB, float* worldSpaceDim, bool square = false, bool roundToPixelSize = false, bool useConstantSize = false) {
		// multiply by inverse projection*view matrix to find frustum vertices in world space
		// transform to light space
		// same pass, find minimum along each axis
		glm::mat4 lightSpaceTransform = glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), -glm::vec3(lightDirection), glm::vec3(0.0f, 1.0f, 0.0f));

		bool firstProcessed = false;
		glm::vec3 boundingA(std::numeric_limits<float>::infinity());
		glm::vec3 boundingB(-std::numeric_limits<float>::infinity());

		// start with <-1 -1 -1> to <1 1 1> cube
		std::vector<glm::vec4> boundingVertices = {
			{-1.0f,	-1.0f,	-1.0f,	1.0f},
			{-1.0f,	-1.0f,	1.0f,	1.0f},
			{-1.0f,	1.0f,	-1.0f,	1.0f},
			{-1.0f,	1.0f,	1.0f,	1.0f},
			{1.0f,	-1.0f,	-1.0f,	1.0f},
			{1.0f,	-1.0f,	1.0f,	1.0f},
			{1.0f,	1.0f,	-1.0f,	1.0f},
			{1.0f,	1.0f,	1.0f,	1.0f}
		};
		for (glm::vec4& vert : boundingVertices) {
			// clip space -> world space
			vert = frustumMat * vert;
			vert /= vert.w;
		}

		for (glm::vec4 vert : boundingVertices) {
			// clip space -> world space -> light space
			vert = lightSpaceTransform * vert;

			// initialize bounds without comparison, only for first transformed vertex
			if (!firstProcessed) {
				boundingA = glm::vec3(vert);
				boundingB = glm::vec3(vert);
				firstProcessed = true;
				continue;
			}

			// expand bounding box to encompass everything in 3D
			boundingA.x = std::min(vert.x, boundingA.x);
			boundingB.x = std::max(vert.x, boundingB.x);
			boundingA.y = std::min(vert.y, boundingA.y);
			boundingB.y = std::max(vert.y, boundingB.y);
			boundingA.z = std::min(vert.z, boundingA.z);
			boundingB.z = std::max(vert.z, boundingB.z);
		}

		// fit z bounding to scene AABB
		for (uint32_t i = 0; i < 2; i++) {
			for (uint32_t j = 0; j < 2; j++) {
				for (uint32_t k = 0; k < 2; k++) {
					glm::vec3 vert = glm::vec3(lightSpaceTransform * sceneAABB * glm::vec4(i, j, k, 1));
					boundingA.z = std::min(vert.z, boundingA.z);
					boundingB.z = std::max(vert.z, boundingB.z);
				}
			}
		}

		// from https://en.wikipedia.org/wiki/Orthographic_projection#Geometry
		// because I don't trust GLM
		float l = boundingA.x;
		float r = boundingB.x;
		float b = boundingA.y;
		float t = boundingB.y;
		float n = boundingA.z;
		float f = boundingB.z;

		float actualSize = std::max(r - l, t - b);
		if (useConstantSize) {
			// keep constant world-size resolution, side length = diagonal of largest face of frustum
			// the other option looks good at high resolutions, but can result in shimmering as you look in different directions and the cascade changes size
			 actualSize = glm::length(glm::vec3(boundingVertices[7]) - glm::vec3(boundingVertices[1]));
		}

		*worldSpaceDim = actualSize;

		// make it square
		if (square) {
			float W = r - l, H = t - b;
			float diff = actualSize - H;
			if (diff > 0) {
				t += diff / 2.0f;
				b -= diff / 2.0f;
			}
			diff = actualSize - W;
			if (diff > 0) {
				r += diff / 2.0f;
				l -= diff / 2.0f;
			}
		}

		// avoid shimmering
		if (roundToPixelSize) {
			float pixelSize = actualSize / dim;
			l = std::round(l / pixelSize) * pixelSize;
			r = std::round(r / pixelSize) * pixelSize;
			b = std::round(b / pixelSize) * pixelSize;
			t = std::round(t / pixelSize) * pixelSize;
		}

		glm::mat4 ortho = {
			2.0f / (r - l),	0.0f,			0.0f,			0.0f,
			0.0f,			2.0f / (t - b),	0.0f,			0.0f,
			0.0f,			0.0f,			2.0f / (f - n),	0.0f,
			-(r + l) / (r - l),	-(t + b) / (t - b),	-(f + n) / (f - n),	1.0f
		};
		ortho = glm::mat4{
			1,0,0,0,
			0,1,0,0,
			0,0,.5f,0,
			0,0,.5f,1
		} *ortho;

		// project in light space -> world space
		ortho = ortho * lightSpaceTransform;

		return  ortho;
	}
}