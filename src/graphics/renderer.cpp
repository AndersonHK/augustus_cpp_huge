#include "renderer.h"

static const graphics_renderer_interface *renderer;

const graphics_renderer_interface *graphics_renderer(void)
{
    return renderer;
}

void graphics_renderer_set_interface(const graphics_renderer_interface *new_renderer)
{
    renderer = new_renderer;
}

void graphics_renderer_begin_command_recording(void)
{
    if (renderer && renderer->begin_command_recording) renderer->begin_command_recording();
}

void graphics_renderer_end_command_recording(void)
{
    if (renderer && renderer->end_command_recording) renderer->end_command_recording();
}

renderer_command_snapshot graphics_renderer_command_snapshot(void)
{
    return renderer && renderer->command_snapshot ? renderer->command_snapshot() : renderer_command_snapshot{};
}

renderer_command_snapshot_handle graphics_renderer_acquire_command_snapshot(void)
{
    return renderer && renderer->acquire_command_snapshot ? renderer->acquire_command_snapshot() : renderer_command_snapshot_handle{};
}

void graphics_renderer_release_command_snapshot(renderer_command_snapshot_handle *snapshot)
{
    if (renderer && renderer->release_command_snapshot) renderer->release_command_snapshot(snapshot);
}

void graphics_renderer_replay_recorded_commands(void)
{
    if (renderer && renderer->replay_recorded_commands) renderer->replay_recorded_commands();
}
