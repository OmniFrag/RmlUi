#include "DataView.h"
#include "../../Include/RmlUi/Core/Element.h"
#include <algorithm>

namespace Rml {

DataView::~DataView() {}

Element* DataView::GetElement() const
{
	Element* result = attached_element.get();
	if (!result)
		Log::Message(Log::LT_WARNING, "Could not retrieve element in view, was it destroyed?");
	return result;
}

int DataView::GetSortOrder() const
{
	return sort_order;
}

bool DataView::IsValid() const
{
	return static_cast<bool>(attached_element);
}

void DataView::MarkRemoved()
{
	attached_element.reset();
}

DataView::DataView(Element* element, int bias) : attached_element(element->GetObserverPtr()), sort_order(bias + 1000)
{
	RMLUI_ASSERT(bias >= -1000 && bias <= 999);

	if (element)
	{
		for (Element* parent = element->GetParentNode(); parent; parent = parent->GetParentNode())
			sort_order += 2000;
	}
}

DataViews::DataViews() {}

DataViews::~DataViews() {}

void DataViews::Add(DataViewPtr view)
{
	views_to_add.push_back(std::move(view));
}

void DataViews::OnElementRemove(Element* element)
{
	// A removed view stays listed until the end of the next update, where it would otherwise update while its element lives
	// on out of the model (a closed document until its release), and a data-for there would give rows aliases that outlive
	// them; one still to be added would be added for good.
	for (auto it = views.begin(); it != views.end();)
	{
		auto& view = *it;
		if (view && view->GetElement() == element)
		{
			view->MarkRemoved();
			views_to_remove.push_back(std::move(view));
			it = views.erase(it);
		}
		else
			++it;
	}
	views_to_add.erase(std::remove_if(views_to_add.begin(), views_to_add.end(),
						   [element](const DataViewPtr& view) { return view && view->GetElement() == element; }),
		views_to_add.end());
}

bool DataViews::Update(DataModel& model, const DirtyVariables& dirty_variables)
{
	bool result = false;
	size_t num_dirty_variables_prev = 0;

	// View updates may result in newly added views, or even new dirty variables. Thus, we do the
	// update recursively but with an upper limit. Without the loop, newly added views won't be
	// updated until the next Update() call.
	for (int i = 0; (i == 0 || !views_to_add.empty() || num_dirty_variables_prev != dirty_variables.size()) && i < 10; i++)
	{
		num_dirty_variables_prev = dirty_variables.size();

		Vector<DataView*> dirty_views;

		if (!views_to_add.empty())
		{
			views.reserve(views.size() + views_to_add.size());
			for (auto&& view : views_to_add)
			{
				dirty_views.push_back(view.get());
				for (const String& variable_name : view->GetVariableNameList())
					name_view_map.emplace(variable_name, view.get());

				views.push_back(std::move(view));
			}
			views_to_add.clear();
		}

		for (const String& variable_name : dirty_variables)
		{
			// The views reading at the dirty path or below it ("player" and "player.health" for "player")...
			for (auto it = name_view_map.lower_bound(variable_name);
				it != name_view_map.end() && it->first.compare(0, variable_name.size(), variable_name) == 0; ++it)
			{
				if (it->first.size() == variable_name.size() || it->first[variable_name.size()] == '.')
					dirty_views.push_back(it->second);
			}
			// ...and those reading what holds it ("vote" for "vote.team"), whose value includes it.
			for (size_t dot = variable_name.rfind('.'); dot != String::npos && dot > 0; dot = variable_name.rfind('.', dot - 1))
			{
				auto pair = name_view_map.equal_range(std::string_view(variable_name).substr(0, dot));
				for (auto it = pair.first; it != pair.second; ++it)
					dirty_views.push_back(it->second);
			}
		}

		// Remove duplicate entries
		std::sort(dirty_views.begin(), dirty_views.end());
		auto it_remove = std::unique(dirty_views.begin(), dirty_views.end());
		dirty_views.erase(it_remove, dirty_views.end());

		// Sort by the element's depth in the document tree so that any structural changes due to a changed variable are reflected in the element's
		// children. Eg. the 'data-for' view will remove children if any of its data variable array size is reduced.
		std::sort(dirty_views.begin(), dirty_views.end(), [](auto&& left, auto&& right) { return left->GetSortOrder() < right->GetSortOrder(); });

		for (DataView* view : dirty_views)
		{
			RMLUI_ASSERT(view);
			if (!view)
				continue;

			if (view->IsValid())
				result |= view->Update(model);
		}

		// Destroy views marked for destruction, found by the paths they're kept by
		if (!views_to_remove.empty())
		{
			for (const auto& view : views_to_remove)
			{
				for (const String& variable_name : view->GetVariableNameList())
				{
					auto pair = name_view_map.equal_range(variable_name);
					for (auto it = pair.first; it != pair.second;)
					{
						if (it->second == view.get())
							it = name_view_map.erase(it);
						else
							++it;
					}
				}
			}

			views_to_remove.clear();
		}
	}

	return result;
}

} // namespace Rml
