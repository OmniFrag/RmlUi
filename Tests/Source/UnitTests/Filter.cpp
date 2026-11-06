#include "../Common/TestsInterface.h"
#include "../Common/TestsShell.h"
#include <RmlUi/Core/CompiledFilterShader.h>
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Core.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Factory.h>
#include <RmlUi/Core/Filter.h>
#include <RmlUi/Core/PropertyDefinition.h>
#include <RmlUi/Core/PropertyDictionary.h>
#include <RmlUi/Core/RenderManager.h>
#include <algorithm>
#include <doctest.h>

using namespace Rml;
class FilterTest;

struct CompiledTestFilter {
	String element_id;
	const FilterTest* filter;
};
static Vector<CompiledTestFilter> compiled_test_filters;

class FilterTest : public Filter {
public:
	FilterTest(float value, Unit unit) : value(value), unit(unit) {}

	CompiledFilter CompileFilter(Element* element) const override
	{
		compiled_test_filters.push_back({element->GetId(), this});
		return element->GetRenderManager()->CompileFilter("FilterTest", {});
	}

	float value = 0.f;
	Unit unit = Unit::UNKNOWN;
};

class FilterTestInstancer : public FilterInstancer {
public:
	enum class ValueType { NumberPercent, Angle };

	FilterTestInstancer()
	{
		id = RegisterProperty("value", "10cm").AddParser("length").GetId();
		RegisterShorthand("filter", "value", ShorthandType::FallThrough);
	}

	SharedPtr<Filter> InstanceFilter(const String& /*name*/, const PropertyDictionary& properties) override
	{
		const Property* p_value = properties.GetProperty(id);
		if (!p_value)
			return nullptr;

		CHECK(Any(p_value->unit & Unit::LENGTH));
		num_instances += 1;

		return MakeShared<FilterTest>(p_value->Get<float>(), p_value->unit);
	}

	int num_instances = 0;

private:
	PropertyId id = {};
};

static const String document_filter_rml = R"(
<rml>
<head>
	<style>
		body {
			top: 100px;
			left: 200px;
			width: 800px;
			height: 600px;
		}
		div {
			display: block;
			width: 400px;
			height: 300px;
		}
		#a    { filter: test(); }
		#b    { filter: test(0); }
		#c,#d { filter: test(1dp); }
	</style>
</head>

<body>
	<div id="a"/>
	<div id="b"/>
	<div id="c"/>
	<div id="d"/>
</body>
</rml>
)";

TEST_CASE("filter")
{
	compiled_test_filters.clear();
	Context* context = TestsShell::GetContext();
	REQUIRE(context);

	FilterTestInstancer instancer;
	Rml::Factory::RegisterFilterInstancer("test", &instancer);

	ElementDocument* document = context->LoadDocumentFromMemory(document_filter_rml);
	document->Show();

	TestsShell::RenderLoop();

	struct ExpectedCompiledFilter {
		float value;
		Unit unit;
	};

	// Map element ID to its expected filter value and unit.
	UnorderedMap<String, ExpectedCompiledFilter> expected_compiled_filters = {
		{"a", {10.f, Unit::CM}},
		{"b", {0.f, Unit::PX}},
		{"c", {1.f, Unit::DP}},
		{"d", {1.f, Unit::DP}},
	};
	for (const auto& compiled_filter : compiled_test_filters)
	{
		INFO("ID #", compiled_filter.element_id);
		auto it = expected_compiled_filters.find(compiled_filter.element_id);

		const bool id_found = (it != expected_compiled_filters.end());
		CHECK(id_found);
		if (!id_found)
			continue;

		const ExpectedCompiledFilter& expected = it->second;
		CHECK(compiled_filter.filter->value == expected.value);
		CHECK(compiled_filter.filter->unit == expected.unit);
	}

	// Check that filters are not compiled more than once for each element.
	CHECK(compiled_test_filters.size() == expected_compiled_filters.size());

	// Filters aren't cached like decorators are, so each element will instance a new decorator even if they refer to
	// the same style rule. Thus, here producing 4 instead of 3 unique instances.
	CHECK(instancer.num_instances == expected_compiled_filters.size());

	auto& counters = TestsShell::GetTestsRenderInterface()->GetCounters();
	CHECK(counters.compile_filter == expected_compiled_filters.size());
	CHECK(counters.release_filter == 0);

	document->Close();
	context->Update();

	CHECK(counters.compile_filter == expected_compiled_filters.size());
	CHECK(counters.release_filter == expected_compiled_filters.size());

	TestsShell::ShutdownShell();
}

static const String document_filter_box_rml = R"(
<rml>
<head>
	<style>
		body {
			width: 800px;
			height: 600px;
			--vw-filter: test(1vw);
		}
		div {
			position: absolute;
			left: 10px;
			top: 10px;
			width: 100px;
			height: 100px;
		}
		#box { filter: test(1dp); }
		#vw  { filter: test(1vw); }
		#var { filter: var(--vw-filter); }
	</style>
</head>

<body>
	<div id="box"/>
	<div id="vw"/>
	<div id="var"/>
</body>
</rml>
)";

TEST_CASE("filter.box_change")
{
	compiled_test_filters.clear();
	Context* context = TestsShell::GetContext();
	REQUIRE(context);

	FilterTestInstancer instancer;
	Rml::Factory::RegisterFilterInstancer("test", &instancer);

	ElementDocument* document = context->LoadDocumentFromMemory(document_filter_box_rml);
	document->Show();
	TestsShell::RenderLoop();

	auto num_compiled = [](const String& id) {
		return std::count_if(compiled_test_filters.begin(), compiled_test_filters.end(),
			[&id](const CompiledTestFilter& compiled) { return compiled.element_id == id; });
	};
	REQUIRE(num_compiled("box") == 1);
	REQUIRE(num_compiled("vw") == 1);
	REQUIRE(num_compiled("var") == 1);

	// A new position or size gives the element a new box, which its filters don't depend on.
	Element* box = document->GetElementById("box");
	box->SetProperty("left", "20.5px");
	box->SetProperty("width", "150.5px");
	TestsShell::RenderLoop();
	CHECK(num_compiled("box") == 1);

	// The viewport's size changes what a length relative to it resolves to, so filters using such lengths are compiled
	// again, including those whose lengths are only known once their variables are substituted.
	const Vector2i dimensions = context->GetDimensions();
	context->SetDimensions(dimensions + Vector2i(100, 50));
	TestsShell::RenderLoop();
	CHECK(num_compiled("vw") == 2);
	CHECK(num_compiled("var") == 2);
	CHECK(num_compiled("box") == 1);

	// So does a new value.
	box->SetProperty("filter", "test(2dp)");
	TestsShell::RenderLoop();
	CHECK(num_compiled("box") == 2);

	context->SetDimensions(dimensions);
	document->Close();
	TestsShell::ShutdownShell();
}

// Records the layer calls, and composites with info only when told to.
class LayerRecordingRenderInterface : public TestsRenderInterface {
public:
	LayerHandle PushLayer() override
	{
		calls.push_back("push");
		return LayerHandle(1);
	}
	void CompositeLayers(LayerHandle source, LayerHandle destination, BlendMode /*blend_mode*/,
		Span<const CompiledFilterHandle> filters) override
	{
		calls.push_back(CreateString("composite %d>%d, %d filters", int(source), int(destination), int(filters.size())));
	}
	bool CompositeLayersWithInfo(LayerHandle source, LayerHandle destination, BlendMode /*blend_mode*/,
		Span<const CompiledFilterHandle> filters, const CompositeInfo& info) override
	{
		if (!support_info)
			return false;
		const Rectanglei& input = info.input_region;
		calls.push_back(CreateString("composite %d>%d, %d filters, from %d,%d %dx%d", int(source), int(destination), int(filters.size()),
			input.Left(), input.Top(), input.Width(), input.Height()));
		element_shape = info.element_shape;
		return true;
	}
	void PopLayer() override
	{
		calls.push_back("pop");
	}

	bool support_info = false;
	StringList calls;
	ClipShape element_shape = {};
};

static const String document_backdrop_rml = R"(
<rml>
<head>
	<style>
		div {
			position: absolute;
			left: 100px;
			top: 50px;
			width: 200px;
			height: 100px;
			border-radius: 10px 20px 0 5px;
			backdrop-filter: blur(10px);
		}
	</style>
</head>

<body>
	<div/>
</body>
</rml>
)";

TEST_CASE("filter.backdrop")
{
	REQUIRE(TestsShell::GetContext());

	for (const bool support_info : {true, false})
	{
		INFO("Composite with info: ", support_info);
		LayerRecordingRenderInterface render_interface;
		render_interface.support_info = support_info;

		Context* context = Rml::CreateContext("backdrop", Vector2i(800, 600), &render_interface);
		REQUIRE(context);
		ElementDocument* document = context->LoadDocumentFromMemory(document_backdrop_rml);
		document->Show();
		context->Update();
		context->Render();

		// The blur reads three sigma past the border box. With info, the window is composited onto itself in one step,
		// reading from that region; without it, through a temporary layer.
		if (support_info)
		{
			CHECK(render_interface.calls == StringList{"composite 0>0, 1 filters, from 70,20 260x160"});

			// The element's border box and its corners, for a renderer to clip the backdrop to.
			const ClipShape& shape = render_interface.element_shape;
			CHECK(shape.rectangle == Rectanglef::FromPositionSize({100.f, 50.f}, {200.f, 100.f}));
			CHECK(shape.radii == Array<Vector2f, 4>{Vector2f(10.f), Vector2f(20.f), Vector2f(0.f), Vector2f(5.f)});
		}
		else
			CHECK(render_interface.calls == StringList{"push", "composite 0>1, 1 filters", "composite 1>0, 0 filters", "pop"});

		Rml::RemoveContext("backdrop");
		Rml::ReleaseRenderManagers();
	}

	TestsShell::ShutdownShell();
}
