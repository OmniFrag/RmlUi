#include "../Common/TestsInterface.h"
#include "../Common/TestsShell.h"
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Core.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/StringUtilities.h>
#include <doctest.h>

using namespace Rml;

// Records the clip shapes it is offered, and clips to them only when told to.
class ClipShapeRecordingRenderInterface : public TestsRenderInterface {
public:
	bool RenderToClipShape(ClipMaskOperation operation, CompiledGeometryHandle geometry, const ClipShape& shape, Vector2f translation) override
	{
		CHECK(geometry != 0);
		const Rectanglef& r = shape.rectangle;
		String radii;
		for (const Vector2f& radius : shape.radii)
			radii += (radius.x == radius.y ? CreateString(" %g", radius.x) : CreateString(" %gx%g", radius.x, radius.y));
		shapes.push_back(CreateString("%s %g,%g %gx%g, radii%s, at %g,%g%s", operation == ClipMaskOperation::Set ? "set" : "intersect", r.Left(),
			r.Top(), r.Width(), r.Height(), radii.c_str(), translation.x, translation.y, transformed ? ", transformed" : ""));
		return support_shapes && (support_intersect || operation == ClipMaskOperation::Set);
	}

	void SetTransform(const Matrix4f* transform) override
	{
		transformed = (transform != nullptr);
		TestsRenderInterface::SetTransform(transform);
	}

	bool support_shapes = false;
	bool support_intersect = true;
	bool transformed = false;
	StringList shapes;
};

static String JoinShapes(const StringList& shapes)
{
	String result;
	for (const String& shape : shapes)
		result += "\n  " + shape;
	return result;
}

// Renders the document once, with the given style on its element of id 'clip', counting the render interface's calls.
static void RenderDocument(ClipShapeRecordingRenderInterface& render_interface, const String& document_rml, const String& clip_style = {})
{
	Context* context = Rml::CreateContext("clip_shape", Vector2i(800, 600), &render_interface);
	REQUIRE(context);
	ElementDocument* document = context->LoadDocumentFromMemory(document_rml);
	REQUIRE(document);
	if (!clip_style.empty())
		document->GetElementById("clip")->SetAttribute("style", clip_style);
	document->Show();
	context->Update();

	render_interface.ResetCounters();
	context->Render();

	Rml::RemoveContext("clip_shape");
	Rml::ReleaseRenderManagers();
}

static const String document_clip_rml = R"(
<rml>
<head>
	<style>
		.clip {
			position: absolute;
			left: 100px;
			top: 50px;
			width: 200px;
			height: 100px;
			overflow: hidden;
		}
		.content {
			height: 300px;
			background-color: #fff;
		}
	</style>
</head>

<body>
	<div class="clip" id="clip"><div class="content"/></div>
</body>
</rml>
)";

TEST_CASE("clip_mask.shape")
{
	REQUIRE(TestsShell::GetContext());

	struct TestCase {
		String clip_style;
		String expected_shape;
	};
	const TestCase test_cases[] = {
		// The clip area is the padding box, inside the border.
		{"border-radius: 20px", "set 0,0 200x100, radii 20 20 20 20, at 100,50"},
		{"border-radius: 10px 20px 30px 0px", "set 0,0 200x100, radii 10 20 30 0, at 100,50"},
		{"border: 5px #000; border-radius: 20px", "set 5,5 200x100, radii 15 15 15 15, at 100,50"},
		// Radii too large for the box are scaled down, as when drawn.
		{"border-radius: 1000px", "set 0,0 200x100, radii 50 50 50 50, at 100,50"},
		// A border wider than the radius leaves a sharp inner corner.
		{"border: 30px #000; border-radius: 20px", "set 30,30 200x100, radii 0 0 0 0, at 100,50"},
		// Unequal border widths make the inner corners elliptic.
		{"border-width: 5px 10px; border-color: #000; border-radius: 20px", "set 10,5 200x100, radii 10x15 10x15 10x15 10x15, at 100,50"},
		// The shape is offered under the clipping element's transform.
		{"border-radius: 20px; transform: rotate(10deg)", "set 0,0 200x100, radii 20 20 20 20, at 100,50, transformed"},
	};

	for (const TestCase& test_case : test_cases)
	{
		for (const bool support_shapes : {true, false})
		{
			INFO("Style: ", test_case.clip_style, ", renderer supports shapes: ", support_shapes);
			ClipShapeRecordingRenderInterface render_interface;
			render_interface.support_shapes = support_shapes;
			RenderDocument(render_interface, document_clip_rml, test_case.clip_style);
			INFO("Shapes offered:", JoinShapes(render_interface.shapes));

			// The shape is offered once, as the clip is set, and replaces the clip mask when the renderer takes it.
			CHECK(render_interface.shapes == StringList{test_case.expected_shape});
			CHECK(render_interface.GetCounters().render_to_clip_mask == (support_shapes ? 0 : 1));
		}
	}

	TestsShell::ShutdownShell();
}

static const String document_nested_clip_rml = R"(
<rml>
<head>
	<style>
		div {
			overflow: hidden;
			border-radius: 10px;
		}
		.outer {
			position: absolute;
			left: 100px;
			top: 50px;
			width: 200px;
			height: 100px;
		}
		.inner {
			width: 100px;
			height: 200px;
			background-color: #888;
		}
		.content {
			height: 300px;
			border-radius: 0;
			background-color: #fff;
		}
	</style>
</head>

<body>
	<div class="outer"><div class="inner"><div class="content"/></div></div>
</body>
</rml>
)";

TEST_CASE("clip_mask.shape_nested")
{
	REQUIRE(TestsShell::GetContext());

	// The inner element is clipped by the outer one alone, and its content by both, the nearest first.
	const StringList expected_shapes = {
		"set 0,0 200x100, radii 10 10 10 10, at 100,50",
		"set 0,0 100x200, radii 10 10 10 10, at 100,50",
		"intersect 0,0 200x100, radii 10 10 10 10, at 100,50",
	};

	ClipShapeRecordingRenderInterface render_interface;
	render_interface.support_shapes = true;
	RenderDocument(render_interface, document_nested_clip_rml);
	INFO("Shapes offered:", JoinShapes(render_interface.shapes));
	CHECK(render_interface.shapes == expected_shapes);
	CHECK(render_interface.GetCounters().render_to_clip_mask == 0);

	// A renderer that takes only the first shape declines the second: the clip mask is enabled again, and both geometries are
	// rendered to it instead.
	ClipShapeRecordingRenderInterface set_only_render_interface;
	set_only_render_interface.support_shapes = true;
	set_only_render_interface.support_intersect = false;
	RenderDocument(set_only_render_interface, document_nested_clip_rml);
	INFO("Shapes offered:", JoinShapes(set_only_render_interface.shapes));
	CHECK(set_only_render_interface.shapes == expected_shapes);
	CHECK(set_only_render_interface.GetCounters().render_to_clip_mask == 2);

	TestsShell::ShutdownShell();
}
