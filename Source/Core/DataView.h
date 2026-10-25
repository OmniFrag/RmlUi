#pragma once

#include "../../Include/RmlUi/Core/DataTypes.h"
#include "../../Include/RmlUi/Core/Header.h"
#include "../../Include/RmlUi/Core/Traits.h"
#include "../../Include/RmlUi/Core/Types.h"
#include <map>
#include <string_view>

namespace Rml {

class Element;
class DataModel;

class DataViewInstancer : public NonCopyMoveable {
public:
	DataViewInstancer() {}
	virtual ~DataViewInstancer() {}
	virtual DataViewPtr InstanceView(Element* element) = 0;
};

template <typename T>
class DataViewInstancerDefault final : public DataViewInstancer {
public:
	DataViewPtr InstanceView(Element* element) override { return DataViewPtr(new T(element)); }
};

/**
    Data view.

    Data views are used to present a data variable in the document by different means.
    A data view is declared in the document by the element attribute:

        data-[type]-[modifier]="[expression]"

    The modifier may or may not be required depending on the data view.
 */

class DataView : public Releasable {
public:
	virtual ~DataView();

	// Initialize the data view.
	// @param[in] model The data model the view will be attached to.
	// @param[in] element The element which spawned the view.
	// @param[in] expression The value of the element's 'data-' attribute which spawned the view (see class documentation).
	// @param[in] modifier The modifier for the given view type (see class documentation).
	// @return True on success.
	virtual bool Initialize(DataModel& model, Element* element, const String& expression, const String& modifier) = 0;

	// Update the data view.
	// Returns true if the update resulted in a document change.
	virtual bool Update(DataModel& model) = 0;

	// Returns the list of data variable name(s) which can modify this view.
	virtual StringList GetVariableNameList() const = 0;

	// Returns the attached element if it still exists.
	Element* GetElement() const;

	// Data views are first sorted by the depth of the attached element in the
	// document tree, then optionally by an offset specified for each data view.
	int GetSortOrder() const;

	// Returns true if the element still exists.
	bool IsValid() const;

	// Lets go of the element, as it leaves the data model: the view isn't valid from then on, even while the element lives on
	// out of the model, as a closed document does until it's released.
	void MarkRemoved();

protected:
	// @param[in] element The element this data view is attached to.
	// @param[in] sort_offset A number [-1000, 999] specifying the update order of this
	//            data view at the same tree depth, negative numbers are updated first.
	DataView(Element* element, int sort_offset);

private:
	ObserverPtr<Element> attached_element;
	int sort_order;
};

class DataViews : NonCopyMoveable {
public:
	DataViews();
	~DataViews();

	void Add(DataViewPtr view);

	void OnElementRemove(Element* element);

	bool Update(DataModel& model, const DirtyVariables& dirty_variables);

private:
	using DataViewList = Vector<DataViewPtr>;

	DataViewList views;

	DataViewList views_to_add;
	DataViewList views_to_remove;

	// Each view by the paths it reads (DataAddressPath), in order, so that a dirty path finds the views at it, below it
	// and above it.
	using NameViewMap = std::multimap<String, DataView*, std::less<>>;
	NameViewMap name_view_map;
};

// The path a data address reads, which its view is kept by: its names up to its first index, joined by dots, as in
// "player.health" for player.health and "match.players" for match.players[3].name. A view is updated when the path, a
// path below it, or one above it is dirtied: a data model's variable dirtied as a whole updates every view reading
// inside it, and a member dirtied alone only those reading it, so a variable with many members needn't update every
// view of it for each change.
inline String DataAddressPath(const DataAddress& address)
{
	String path;
	for (const DataAddressEntry& entry : address)
	{
		if (entry.index >= 0 || entry.name.empty())
			break;
		if (!path.empty())
			path += '.';
		path += entry.name;
	}
	return path;
}

} // namespace Rml
