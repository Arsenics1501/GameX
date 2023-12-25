#include "GameX/animation/manager.h"

#include "GameX/application/application.h"

namespace GameX::Animation {
Manager::Manager(Base::Renderer *renderer) : renderer_(renderer) {
  auto extent = renderer_->App()->VkCore()->SwapChain()->Extent();
  film_ = renderer_->RenderPipeline()->CreateFilm(extent.width, extent.height);
}

Manager::~Manager() {
  ProcessCommandBufferQueue();
  film_.reset();
}

void Manager::Update(float delta_time) {
  ProcessCommandBufferQueue();
  for (auto &object : dynamic_objects_) {
    object->Update(delta_time);
  }
}

void Manager::RecordCommand(const std::function<void()> &command) {
  if (working_cmd_buffer_) {
    working_cmd_buffer_->commands.push(command);
  }
}

void Manager::RegisterObject(Object *object) {
  if (working_cmd_buffer_) {
    working_cmd_buffer_->new_objects.push(object);
  }
}

void Manager::RegisterDynamicObject(DynamicObject *object) {
  if (working_cmd_buffer_) {
    working_cmd_buffer_->new_dynamic_objects.push(object);
  }
}

void Manager::UnregisterObject(Object *object) {
  if (working_cmd_buffer_) {
    working_cmd_buffer_->delete_objects.push(object);
  }
}

void Manager::UnregisterDynamicObject(DynamicObject *object) {
  dynamic_objects_.erase(object);
}

void Manager::ProcessCommandBufferQueue() {
  std::lock_guard<std::mutex> lock(cmd_buffer_queue_mutex_);

  while (!cmd_buffer_queue_.empty()) {
    auto &cmd_buffer = cmd_buffer_queue_.front();

    while (!cmd_buffer.new_objects.empty()) {
      objects_.insert(cmd_buffer.new_objects.front());
      cmd_buffer.new_objects.pop();
    }

    while (!cmd_buffer.new_dynamic_objects.empty()) {
      dynamic_objects_.insert(cmd_buffer.new_dynamic_objects.front());
      cmd_buffer.new_dynamic_objects.pop();
    }

    while (!cmd_buffer.commands.empty()) {
      cmd_buffer.commands.front()();
      cmd_buffer.commands.pop();
    }

    if (!cmd_buffer.delete_objects.empty()) {
      renderer_->App()->VkCore()->Device()->WaitIdle();
      while (!cmd_buffer.delete_objects.empty()) {
        auto &object = cmd_buffer.delete_objects.front();
        objects_.erase(object);
        delete object;
        cmd_buffer.delete_objects.pop();
      }
    }

    cmd_buffer_queue_.pop();
  }
}

bool Manager::Render(VkCommandBuffer cmd_buffer) {
  if (primary_scene_ && primary_camera_) {
    Renderer()->RenderPipeline()->Render(cmd_buffer, *primary_scene_->Handle(),
                                         *primary_camera_->Handle(), *film_);
    return true;
  }
  return false;
}

void Manager::SetPrimarySceneCamera(Scene *scene, Camera *camera) {
  RecordCommand([this, scene, camera]() {
    primary_scene_ = scene;
    primary_camera_ = camera;
  });
}

}  // namespace GameX::Animation
