#pragma once

#include "../../Include/RmlUi/Core/CompiledFilterShader.h"
#include "../../Include/RmlUi/Core/Types.h"

namespace Rml {

class Decorator;
class Element;
class Filter;

enum class RenderStage { Enter, Decoration, Exit };

/**
    Manages and renders an element's effects: decorators, filters, backdrop filters, and mask images.
 */

class ElementEffects {
public:
	ElementEffects(Element* element);
	~ElementEffects();

	void InstanceEffects();

	void RenderEffects(RenderStage render_stage);

	// Mark effects as dirty and force them to reset themselves.
	void DirtyEffects();
	// Mark the element data of effects as dirty, including compiled filters.
	void DirtyEffectsData();
	// Mark the element data of decorators and mask images as dirty, as when the element's box changes. Filters are
	// compiled from the element's properties alone, so they're left as they are.
	void DirtyDecoratorsData();

private:
	// Releases existing element data of effects, and regenerates it.
	void ReloadEffectsData();
	// Releases all existing effects and their element data.
	void ReleaseEffects();

	struct DecoratorEntry {
		SharedPtr<const Decorator> decorator;
		DecoratorDataHandle decorator_data;
		BoxArea paint_area;
	};
	using DecoratorEntryList = Vector<DecoratorEntry>;

	struct FilterEntry {
		SharedPtr<const Filter> filter;
		CompiledFilter compiled;
	};
	using FilterEntryList = Vector<FilterEntry>;

	Element* element;

	// The list of decorators and filters used by this element.
	DecoratorEntryList decorators;
	DecoratorEntryList mask_images;
	FilterEntryList filters;
	FilterEntryList backdrop_filters;

	// If set, a full reload is necessary.
	bool effects_dirty = false;
	// If set, element data of all decorators need to be regenerated, and all filters compiled again.
	bool effects_data_dirty = false;
	// If set, element data of all decorators need to be regenerated.
	bool decorators_data_dirty = false;
};

} // namespace Rml
