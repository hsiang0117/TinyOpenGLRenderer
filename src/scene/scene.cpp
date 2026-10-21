#include "scene.hpp"
#include "component.hpp"
#include "rendering/renderLimits.hpp"
#include "core/logger.hpp"
#include <algorithm>
#include <iterator>

bool Scene::addObject(GameObjectPtr obj)
{
	if (!obj) return false;
	if ((obj->has<PointLightComponent>() && getView<PointLightComponent>().size() >= RenderLimits::PointLights) ||
		(obj->has<DirectionalLightComponent>() && getView<DirectionalLightComponent>().size() >= RenderLimits::DirectionalLights) ||
		(obj->has<SpotLightComponent>() && getView<SpotLightComponent>().size() >= RenderLimits::SpotLights)) {
		LOG_WARN("Cannot add " << obj->getName() << ": light capacity reached");
		return false;
	}
	objects.push_back(obj);
	viewCache_.clear();
	return true;
}

void Scene::removeObject(GameObjectPtr obj)
{
	auto it = std::find(objects.begin(), objects.end(), obj);
	if (it != objects.end()) {
		objects.erase(it);
		viewCache_.clear();
	}
}

void Scene::removeObjectByIndex(std::size_t idx)
{
	if (idx < objects.size()) {
		objects.erase(objects.begin() + idx);
		viewCache_.clear();
	}
}

void Scene::queueRemoval(std::size_t idx)
{
	removalQueue.push(idx);
}

void Scene::processRemovals()
{
	std::vector<std::size_t> indices;
	while (!removalQueue.empty()) {
		indices.push_back(removalQueue.front());
		removalQueue.pop();
	}
	std::sort(indices.rbegin(), indices.rend());

	for (std::size_t idx : indices) {
		if (idx < objects.size()) {
			removeObjectByIndex(idx);
		}
	}
}

std::vector<GameObjectPtr> Scene::getObjects(ObjectFilter filter) const
{
	if (!filter) return objects;

	std::vector<GameObjectPtr> filtered;
	std::copy_if(objects.begin(), objects.end(), std::back_inserter(filtered), filter);
	return filtered;
}

void Scene::shutdown()
{
	objects.clear();
	while (!removalQueue.empty()) removalQueue.pop();
	viewCache_.clear();
}
