#pragma once

#include "figure/figure_type_registry_internal.h"

#include "figure/figure.h"

#include <memory>

namespace figure_runtime_native_impl {

class NativeFigure {
public:
    NativeFigure(
        Figure *data,
        const figure_type_registry_impl::FigureTypeDefinition *definition,
        const figure_type_registry_impl::FigureTypeProfile *profile)
        : figure_(data)
        , definition_(definition)
        , profile_(profile)
    {
    }

    virtual ~NativeFigure() = default;

    void set_figure(Figure *data)
    {
        figure_ = data;
    }

    const figure_type_registry_impl::FigureTypeDefinition *definition() const
    {
        return definition_;
    }

    const figure_type_registry_impl::FigureTypeProfile *profile() const
    {
        return profile_;
    }

    virtual int execute() = 0;

protected:
    Figure *data_figure() const
    {
        return figure_;
    }

private:
    Figure *figure_ = nullptr;
    const figure_type_registry_impl::FigureTypeDefinition *definition_ = nullptr;
    const figure_type_registry_impl::FigureTypeProfile *profile_ = nullptr;
};

// FigureType profiles own native walker behavior; this factory keeps runtime binding separate from controller actions.
std::unique_ptr<NativeFigure> make_controller(
    Figure *f,
    const figure_type_registry_impl::FigureTypeDefinition *definition,
    const figure_type_registry_impl::FigureTypeProfile *profile);

road_service_effect primary_service_effect_for_profile(
    const figure_type_registry_impl::FigureTypeProfile *profile);

bool is_road_history_tile(int grid_offset);

} // namespace figure_runtime_native_impl
