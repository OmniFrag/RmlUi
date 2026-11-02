#include "../../../Source/Core/TransformState.h"
#include "../Common/Mocks.h"
#include "../Common/TestsInterface.h"
#include "../Common/TestsShell.h"
#include "../Common/TypesToString.h"
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/ElementScroll.h>
#include <RmlUi/Core/Factory.h>
#include <doctest.h>

using namespace Rml;

static const String document_clone_rml = R"(
<rml>
<head>
	<title>Test</title>
	<link type="text/rcss" href="/assets/rml.rcss"/>
	<link type="text/rcss" href="/assets/invader.rcss"/>
	<style>
		body
		{
			left: 0;
			top: 0;
			right: 0;
			bottom: 0;
		}
		div {
			drag: clone;
			background-color: #fff;
			height: 100px;
		}
		div.blue {
			background-color: #00f;
		}
		span {
			color: red;
		}
	</style>
</head>

<body>
<div style="background-color: #f00">This is a <span>sample</span>.</div>
</body>
</rml>
)";

static const String document_scroll_rml = R"(
<rml>
<head>
	<title>Test</title>
	<style>
		body {
			left: 0;
			top: 0;
			width: 100px;
			height: 100px;
			padding: 0;
			margin: 0;
			overflow: scroll;
		}
		div {
			display: block;
			width: 200px;
			height: 50px;
			padding: 0;
			margin: 0;
		}
		#row0 { background: #3a3; }
		#row1 { background: #aa3; }
		#row2 { background: #3aa; }
		#row3 { background: #aaa; }
		span {
			display: inline-block;
			box-sizing: border-box;
			width: 50px;
			height: 50px;
			padding: 0;
			margin: 0;
			border: 1px #000;
		}
		span:nth-child(2) { border-color: #fff; }
		span:nth-child(3) { border-color: #f00; }
		span:nth-child(4) { border-color: #ff0; }
		scrollbarvertical,
		scrollbarhorizontal {
			width: 0;
			height: 0;
		}
	</style>
</head>

<body id="scrollable">
<div id="row0">
	<span id="cell00"></span>
	<span id="cell01"></span>
	<span id="cell02"></span>
	<span id="cell03"></span>
</div>
<div id="row1">
	<span id="cell10"></span>
	<span id="cell11"></span>
	<span id="cell12"></span>
	<span id="cell13"></span>
</div>
<div id="row2">
	<span id="cell20"></span>
	<span id="cell21"></span>
	<span id="cell22"></span>
	<span id="cell23"></span>
</div>
<div id="row3">
	<span id="cell30"></span>
	<span id="cell31"></span>
	<span id="cell32"></span>
	<span id="cell33"></span>
</div>
</body>
</rml>
)";

TEST_CASE("Element")
{
	Context* context = TestsShell::GetContext();
	REQUIRE(context);

	ElementDocument* document = context->LoadDocumentFromMemory(document_clone_rml);
	REQUIRE(document);
	document->Show();

	context->Update();
	context->Render();

	Element* div = document->GetFirstChild();
	Element* span = div->GetChild(1);
	REQUIRE(div);
	REQUIRE(div->GetTagName() == "div");
	REQUIRE(span);
	REQUIRE(span->GetTagName() == "span");

	SUBCASE("Attribute")
	{
		auto* button = document->AppendChild(document->CreateElement("button"));
		SUBCASE("Event listener")
		{
			namespace tl = trompeloeil;
			static constexpr auto ON_CLICK_ATTRIBUTE = "onclick";
			static constexpr auto ON_CLICK_VALUE = "moo";

			std::vector<UniquePtr<tl::expectation>> expectations;

			UniquePtr<MockEventListener> mockEventListener;
			const auto configureMockEventListener = [&]() {
				mockEventListener.reset(new MockEventListener());
				expectations.emplace_back(NAMED_ALLOW_CALL(*mockEventListener, OnAttach(button)));
				expectations.emplace_back(NAMED_ALLOW_CALL(*mockEventListener, OnDetach(button)).LR_SIDE_EFFECT(mockEventListener.reset()));
			};

			MockEventListenerInstancer mockEventListenerInstancer;
			const auto configureMockEventListenerInstancer = [&](const auto value) {
				expectations.emplace_back(NAMED_REQUIRE_CALL(mockEventListenerInstancer, InstanceEventListener(value, button))
						.LR_SIDE_EFFECT(configureMockEventListener())
						.LR_RETURN(mockEventListener.get()));
			};

			Factory::RegisterEventListenerInstancer(&mockEventListenerInstancer);

			configureMockEventListenerInstancer(ON_CLICK_VALUE);
			button->SetAttribute(ON_CLICK_ATTRIBUTE, ON_CLICK_VALUE);

			SUBCASE("Replacement")
			{
				static constexpr auto REPLACEMENT_ON_CLICK_VALUE = "boo";

				configureMockEventListenerInstancer(REPLACEMENT_ON_CLICK_VALUE);
				button->SetAttribute(ON_CLICK_ATTRIBUTE, REPLACEMENT_ON_CLICK_VALUE);
			}

			button->RemoveAttribute(ON_CLICK_ATTRIBUTE);
		}

		SUBCASE("Simple")
		{
			static constexpr auto DISABLED_ATTRIBUTE = "disabled";

			REQUIRE_FALSE(button->HasAttribute(DISABLED_ATTRIBUTE));
			button->SetAttribute(DISABLED_ATTRIBUTE, "");
			REQUIRE(button->HasAttribute(DISABLED_ATTRIBUTE));

			SUBCASE("Replacement")
			{
				static constexpr auto VALUE = "something";

				button->SetAttribute(DISABLED_ATTRIBUTE, VALUE);
				const auto* attributeValue = button->GetAttribute(DISABLED_ATTRIBUTE);
				REQUIRE(attributeValue);
				REQUIRE(attributeValue->GetType() == Variant::Type::STRING);
				CHECK(attributeValue->Get<String>() == VALUE);
			}

			button->RemoveAttribute(DISABLED_ATTRIBUTE);
			CHECK_FALSE(button->HasAttribute(DISABLED_ATTRIBUTE));
		}
	}

	SUBCASE("CloneDrag")
	{
		// Simulate input for mouse click and drag
		context->ProcessMouseMove(10, 10, 0);

		context->Update();
		context->Render();

		context->ProcessMouseButtonDown(0, 0);

		context->Update();
		context->Render();

		// This should initiate a drag clone.
		context->ProcessMouseMove(10, 11, 0);

		context->Update();
		context->Render();

		context->ProcessMouseButtonUp(0, 0);
	}

	SUBCASE("CloneManual")
	{
		Element* element = document->GetFirstChild();
		REQUIRE(element->GetProperty<String>("background-color") == "#ff0000");
		CHECK(element->Clone()->GetProperty<String>("background-color") == "#ff0000");

		element->SetProperty("background-color", "#0f0");
		CHECK(element->Clone()->GetProperty<String>("background-color") == "#00ff00");

		element->RemoveProperty("background-color");
		Element* clone = document->AppendChild(element->Clone());
		context->Update();
		CHECK(clone->GetProperty<String>("background-color") == "#ffffff");

		element->SetClass("blue", true);
		clone = document->AppendChild(element->Clone());
		context->Update();
		CHECK(clone->GetProperty<String>("background-color") == "#0000ff");
	}

	SUBCASE("SetInnerRML")
	{
		Element* element = document->GetFirstChild();
		CHECK(element->GetInnerRML() == "This is a <span>sample</span>.");
		element->SetInnerRML("text");
		CHECK(element->GetInnerRML() == "text");

		const char* inner_rml = R"(before<div class="blue">child</div>after)";
		element->SetInnerRML(inner_rml);
		CHECK(element->GetInnerRML() == inner_rml);

		ElementPtr element_ptr = document->CreateElement("div");
		CHECK(element_ptr->GetInnerRML() == "");
		element_ptr->SetInnerRML("text");
		CHECK(element_ptr->GetInnerRML() == "text");
	}

	SUBCASE("GetInnerRML")
	{
		String inner_rml;

		inner_rml = document->GetInnerRML();
		CHECK(inner_rml == R"(<div style="background-color: #ff0000;">This is a <span>sample</span>.</div>)");

		div->SetProperty("background-color", "white");
		inner_rml = document->GetInnerRML();
		CHECK(inner_rml == R"(<div style="background-color: #ffffff;">This is a <span>sample</span>.</div>)");

		div->RemoveProperty("background-color");
		inner_rml = document->GetInnerRML();
		CHECK(inner_rml == R"(<div>This is a <span>sample</span>.</div>)");

		div->SetProperty("cursor", "x<y");
		inner_rml = document->GetInnerRML();
		CHECK(inner_rml == R"(<div style="cursor: x&lt;y;">This is a <span>sample</span>.</div>)");

		span->SetProperty("font-weight", "bold");
		inner_rml = document->GetInnerRML();
		CHECK(inner_rml == R"(<div style="cursor: x&lt;y;">This is a <span style="font-weight: bold;">sample</span>.</div>)");
	}

	SUBCASE("InsertBefore")
	{
		String inner_rml;

		inner_rml = document->GetInnerRML();
		CHECK(inner_rml == R"(<div style="background-color: #ff0000;">This is a <span>sample</span>.</div>)");

		div->InsertBefore(document->CreateElement("img"), span);
		inner_rml = document->GetInnerRML();
		CHECK(inner_rml == R"(<div style="background-color: #ff0000;">This is a <img /><span>sample</span>.</div>)");

		div->InsertBefore(document->CreateElement("button"), nullptr)->SetInnerRML("Click me");
		inner_rml = document->GetInnerRML();
		CHECK(inner_rml == R"(<div style="background-color: #ff0000;">This is a <img /><span>sample</span>.<button>Click me</button></div>)");
	}

	document->Close();
	TestsShell::ShutdownShell();
}

TEST_CASE("Element.ScrollIntoView")
{
	Context* context = TestsShell::GetContext();
	REQUIRE(context);

	ElementDocument* document = context->LoadDocumentFromMemory(document_scroll_rml);
	REQUIRE(document);
	document->Show();

	TestsShell::RenderLoop();

	Element* scrollable = document->GetElementById("scrollable");
	REQUIRE(scrollable);
	Element* cells[4][4]{};
	for (int i = 0; i < 4; ++i)
	{
		for (int j = 0; j < 4; ++j)
		{
			cells[i][j] = document->GetElementById(CreateString("cell%d%d", i, j));
			REQUIRE(cells[i][j]);
		}
	}

	REQUIRE(cells[0][0]->GetAbsoluteOffset(Rml::BoxArea::Border) == Vector2f(0, 0));
	REQUIRE(cells[2][2]->GetAbsoluteOffset(Rml::BoxArea::Border) == Vector2f(100, 100));
	REQUIRE(cells[3][3]->GetAbsoluteOffset(Rml::BoxArea::Border) == Vector2f(150, 150));
	REQUIRE(scrollable->GetScrollLeft() == 0);
	REQUIRE(scrollable->GetScrollTop() == 0);

	SUBCASE("LegacyScroll")
	{
		cells[2][2]->ScrollIntoView(true);

		TestsShell::RenderLoop();

		CHECK(cells[0][0]->GetAbsoluteOffset(Rml::BoxArea::Border) == Vector2f(-50, -100));
		CHECK(cells[2][2]->GetAbsoluteOffset(Rml::BoxArea::Border) == Vector2f(50, 0));
		CHECK(cells[3][3]->GetAbsoluteOffset(Rml::BoxArea::Border) == Vector2f(100, 50));
		CHECK(scrollable->GetScrollLeft() == 50);
		CHECK(scrollable->GetScrollTop() == 100);

		cells[2][2]->ScrollIntoView(false);

		TestsShell::RenderLoop();

		CHECK(cells[0][0]->GetAbsoluteOffset(Rml::BoxArea::Border) == Vector2f(-50, -50));
		CHECK(cells[2][2]->GetAbsoluteOffset(Rml::BoxArea::Border) == Vector2f(50, 50));
		CHECK(cells[3][3]->GetAbsoluteOffset(Rml::BoxArea::Border) == Vector2f(100, 100));
		CHECK(scrollable->GetScrollLeft() == 50);
		CHECK(scrollable->GetScrollTop() == 50);
	}

	SUBCASE("AdvancedScroll")
	{
		cells[2][2]->ScrollIntoView({ScrollAlignment::Center, ScrollAlignment::Center});
		TestsShell::RenderLoop();

		CHECK(cells[0][0]->GetAbsoluteOffset(Rml::BoxArea::Border) == Vector2f(-75, -75));
		CHECK(cells[2][2]->GetAbsoluteOffset(Rml::BoxArea::Border) == Vector2f(25, 25));
		CHECK(cells[3][3]->GetAbsoluteOffset(Rml::BoxArea::Border) == Vector2f(75, 75));

		SUBCASE("NearestAlready")
		{
			cells[2][2]->ScrollIntoView(ScrollAlignment::Nearest);
			TestsShell::RenderLoop();

			CHECK(cells[0][0]->GetAbsoluteOffset(Rml::BoxArea::Border) == Vector2f(-75, -75));
			CHECK(cells[2][2]->GetAbsoluteOffset(Rml::BoxArea::Border) == Vector2f(25, 25));
			CHECK(cells[3][3]->GetAbsoluteOffset(Rml::BoxArea::Border) == Vector2f(75, 75));
		}

		SUBCASE("NearestBefore")
		{
			cells[1][1]->ScrollIntoView(ScrollAlignment::Nearest);
			TestsShell::RenderLoop();

			CHECK(cells[0][0]->GetAbsoluteOffset(Rml::BoxArea::Border) == Vector2f(-50, -50));
			CHECK(cells[1][1]->GetAbsoluteOffset(Rml::BoxArea::Border) == Vector2f(0, 0));
			CHECK(cells[2][2]->GetAbsoluteOffset(Rml::BoxArea::Border) == Vector2f(50, 50));
		}

		SUBCASE("NearestAfter")
		{
			cells[3][3]->ScrollIntoView(ScrollAlignment::Nearest);
			TestsShell::RenderLoop();

			CHECK(cells[1][1]->GetAbsoluteOffset(Rml::BoxArea::Border) == Vector2f(-50, -50));
			CHECK(cells[2][2]->GetAbsoluteOffset(Rml::BoxArea::Border) == Vector2f(0, 0));
			CHECK(cells[3][3]->GetAbsoluteOffset(Rml::BoxArea::Border) == Vector2f(50, 50));
		}

		SUBCASE("Smoothscroll")
		{
			TestsSystemInterface* system_interface = TestsShell::GetTestsSystemInterface();
			system_interface->SetManualTime(0);
			cells[3][3]->ScrollIntoView({ScrollAlignment::Nearest, ScrollAlignment::Nearest, ScrollBehavior::Smooth});

			constexpr double dt = 1.0 / 15.0;
			system_interface->SetManualTime(dt);
			TestsShell::RenderLoop();

			// We don't define the exact offset at this time step, but it should be somewhere between the start and end offsets.
			Vector2f offset = cells[3][3]->GetAbsoluteOffset(Rml::BoxArea::Border);
			CHECK(offset.x > 50.f);
			CHECK(offset.y > 50.f);
			CHECK(offset.x < 75.f);
			CHECK(offset.y < 75.f);

			// After one second it should be at the destination offset.
			for (double t = 2.0 * dt; t < 1.0; t += dt)
			{
				system_interface->SetManualTime(t);
				context->Update();
				context->Render();
			}
			TestsShell::RenderLoop();
			CHECK(cells[3][3]->GetAbsoluteOffset(Rml::BoxArea::Border) == Vector2f(50, 50));
		}
	}

	SUBCASE("Adaptive")
	{
		cells[2][2]->ScrollIntoView({ScrollAlignment::Adaptive, ScrollAlignment::Adaptive});
		CHECK(cells[0][0]->GetAbsoluteOffset(Rml::BoxArea::Border) == Vector2f(-75, -75));
		TestsShell::RenderLoop();

		cells[2][2]->ScrollIntoView({ScrollAlignment::End, ScrollAlignment::End});
		REQUIRE(cells[0][0]->GetAbsoluteOffset(Rml::BoxArea::Border) == Vector2f(-50, -50));

		cells[2][2]->ScrollIntoView({ScrollAlignment::Adaptive, ScrollAlignment::Adaptive});
		CHECK(cells[0][0]->GetAbsoluteOffset(Rml::BoxArea::Border) == Vector2f(-50, -50));

		cells[1][1]->ScrollIntoView({ScrollAlignment::Adaptive, ScrollAlignment::Adaptive});
		CHECK(cells[0][0]->GetAbsoluteOffset(Rml::BoxArea::Border) == Vector2f(-50, -50));
	}

	document->Close();
	TestsShell::ShutdownShell();
}

TEST_CASE("Element.ScrollbarDestruction")
{
	const String document_rml = R"(
<rml>
<head>
	<link type="text/rcss" href="/assets/rml.rcss"/>
	<style>
		#scroll {
			display: block;
			width: 200px;
			height: 100px;
			overflow-y: scroll;
		}
		#scroll div {
			height: 300px;
		}
	</style>
</head>
<body>
	<div id="scroll"><div/></div>
</body>
</rml>
)";

	// Run under address sanitizer to verify that destruction of ElementScroll do not lead to use-after-free within ~Element.
	Context* context = TestsShell::GetContext();
	ElementDocument* document = context->LoadDocumentFromMemory(document_rml);
	document->Show();

	Element* scroll = document->GetElementById("scroll");
	REQUIRE(scroll->GetElementScroll()->GetScrollbar(ElementScroll::VERTICAL) != nullptr);

	scroll->GetParentNode()->RemoveChild(scroll);

	document->Close();
	context->Update();

	TestsShell::ShutdownShell();
}

TEST_CASE("Element.TransformStateAfterVisibilityChange")
{
	const String document_rml = R"(
<rml>
<head>
	<title>Test</title>
	<style>
		body {
			left: 0;
			top: 0;
			width: 400px;
			height: 400px;
		}
		#window {
			position: absolute;
			left: 0;
			top: 0;
			width: 200px;
			height: 200px;
			transform: translateX(100px);
		}
		div.pane {
			display: block;
			width: 50px;
			height: 50px;
		}
		#pane_display { display: none; }
		#pane_visibility { visibility: hidden; }
	</style>
</head>
<body>
	<div id="window">
		<div class="pane" id="pane_reference"/>
		<div class="pane" id="pane_display"/>
		<div class="pane" id="pane_visibility"/>
	</div>
</body>
</rml>
)";

	Context* context = TestsShell::GetContext();
	REQUIRE(context);

	ElementDocument* document = context->LoadDocumentFromMemory(document_rml);
	REQUIRE(document);
	document->Show();

	context->Update();
	context->Render();

	const auto ElementTransform = [](Element* element) -> Matrix4f {
		const TransformState* transform_state = element->GetTransformState();
		const Matrix4f* transform = (transform_state ? transform_state->GetTransform() : nullptr);
		return transform ? *transform : Matrix4f::Identity();
	};

	const Matrix4f window_transform = Matrix4f::TranslateX(100.f);

	Element* window = document->GetElementById("window");
	CHECK(ElementTransform(window) == window_transform);

	Element* pane_reference = document->GetElementById("pane_reference");
	CHECK(ElementTransform(pane_reference) == window_transform);

	// Ensure the correct transform is applied to an element after it becomes visible.
	String pane_id, set_property, set_value;

	SUBCASE("Display")
	{
		pane_id = "pane_display";
		set_property = "display";
		set_value = "block";
	}
	SUBCASE("Visibility")
	{
		pane_id = "pane_visibility";
		set_property = "visibility";
		set_value = "visible";
	}

	Element* pane = document->GetElementById(pane_id);
	CHECK(ElementTransform(pane) == Matrix4f::Identity());

	pane->SetProperty(set_property, set_value);
	context->Update();
	context->Render();

	CHECK(ElementTransform(pane) == window_transform);

	document->Close();
	TestsShell::ShutdownShell();
}

TEST_CASE("Element.IsPointWithinElement.BorderRadius")
{
	const String document_rml = R"(
<rml>
<head>
	<link type="text/rcss" href="/assets/rml.rcss"/>
	<style>
		body { width: 800px; height: 600px; }
		div { position: absolute; top: 100px; }
		#square { left: 0px; width: 100px; height: 100px; }
		#pill { left: 200px; width: 200px; height: 100px; border-radius: 50px; }
		#oversized { left: 500px; width: 100px; height: 60px; border-radius: 200px; }
		#bordered { left: 650px; width: 100px; height: 60px; border: 10px #fff; border-radius: 40px; }
		#rounded_clip { left: 0px; top: 250px; width: 200px; height: 100px; border-radius: 50px; overflow: hidden; }
		#filler, #bordered_filler { position: static; display: block; width: 200px; height: 100px; }
		#bordered_clip { left: 250px; top: 250px; width: 200px; height: 100px; border: 10px #fff; border-radius: 60px; overflow: hidden; }
		#transformed_clip { left: 500px; top: 250px; width: 100px; height: 100px; overflow: hidden; transform: translateX(100px); }
		#overflowing { position: static; display: block; width: 200px; height: 100px; }
		#plain_clip { left: 0px; top: 400px; width: 100px; height: 100px; overflow: hidden; }
		#moved { position: static; display: block; width: 150px; height: 100px; transform: translateX(60px); }
	</style>
</head>
<body>
	<div id="square"/>
	<div id="pill"/>
	<div id="oversized"/>
	<div id="bordered"/>
	<div id="rounded_clip"><div id="filler"/></div>
	<div id="bordered_clip"><div id="bordered_filler"/></div>
	<div id="transformed_clip"><div id="overflowing"/></div>
	<div id="plain_clip"><div id="moved"/></div>
</body>
</rml>
)";

	Context* context = TestsShell::GetContext();
	ElementDocument* document = context->LoadDocumentFromMemory(document_rml);
	REQUIRE(document);
	document->Show();
	context->Update();
	context->Render();

	auto IsWithin = [](Element* element, Vector2f point_from_corner) {
		return element->IsPointWithinElement(element->GetAbsoluteOffset(BoxArea::Border) + point_from_corner);
	};

	SUBCASE("Square corners")
	{
		Element* square = document->GetElementById("square");
		CHECK(IsWithin(square, {1.f, 1.f}));
		CHECK(IsWithin(square, {99.f, 99.f}));
		CHECK_FALSE(IsWithin(square, {101.f, 50.f}));
	}

	SUBCASE("Rounded corners")
	{
		// The left curves are centered at (50, 50) and the right ones at (150, 50), all with radius 50.
		Element* pill = document->GetElementById("pill");
		CHECK(IsWithin(pill, {100.f, 50.f}));
		CHECK(IsWithin(pill, {100.f, 1.f}));
		CHECK(IsWithin(pill, {20.f, 20.f}));
		CHECK(IsWithin(pill, {180.f, 80.f}));
		CHECK_FALSE(IsWithin(pill, {5.f, 5.f}));
		CHECK_FALSE(IsWithin(pill, {195.f, 95.f}));

		// A point in the corner falls through to the element behind.
		const Vector2f origin = pill->GetAbsoluteOffset(BoxArea::Border);
		CHECK(context->GetElementAtPoint(origin + Vector2f(5.f, 5.f)) == document);
		CHECK(context->GetElementAtPoint(origin + Vector2f(100.f, 50.f)) == pill);
	}

	SUBCASE("Radius larger than the box")
	{
		// The radii are scaled down to fit the box, as when drawn: 200px becomes 30px, half the height.
		Element* oversized = document->GetElementById("oversized");
		CHECK(IsWithin(oversized, {50.f, 30.f}));
		CHECK(IsWithin(oversized, {10.f, 10.f}));
		CHECK(IsWithin(oversized, {99.f, 30.f}));
		CHECK_FALSE(IsWithin(oversized, {2.f, 2.f}));
		CHECK_FALSE(IsWithin(oversized, {97.f, 5.f}));
	}

	SUBCASE("Border")
	{
		// Radii are scaled to fit the padding box as when drawn: 40px becomes 30px on this 120x80 border box.
		Element* bordered = document->GetElementById("bordered");
		CHECK(IsWithin(bordered, {60.f, 1.f}));
		CHECK(IsWithin(bordered, {11.f, 11.f}));
		CHECK_FALSE(IsWithin(bordered, {5.f, 5.f}));
		CHECK_FALSE(IsWithin(bordered, {116.f, 76.f}));
	}

	SUBCASE("Clipped by rounded corners")
	{
		// A child filling a rounded container that clips its overflow takes no points in the clipped corners.
		Element* rounded_clip = document->GetElementById("rounded_clip");
		Element* filler = document->GetElementById("filler");
		const Vector2f origin = rounded_clip->GetAbsoluteOffset(BoxArea::Border);
		CHECK(context->GetElementAtPoint(origin + Vector2f(100.f, 50.f)) == filler);
		CHECK(context->GetElementAtPoint(origin + Vector2f(20.f, 20.f)) == filler);
		CHECK(context->GetElementAtPoint(origin + Vector2f(5.f, 5.f)) == document);
		CHECK(context->GetElementAtPoint(origin + Vector2f(195.f, 95.f)) == document);
	}

	SUBCASE("Clipped by rounded corners with a border")
	{
		// The radius becomes 50px to fit the padding box, so the clip's inner curves have radius 40, centered at (50, 50).
		Element* bordered_clip = document->GetElementById("bordered_clip");
		Element* bordered_filler = document->GetElementById("bordered_filler");
		const Vector2f origin = bordered_clip->GetAbsoluteOffset(BoxArea::Border);
		CHECK(context->GetElementAtPoint(origin + Vector2f(110.f, 60.f)) == bordered_filler);
		CHECK(context->GetElementAtPoint(origin + Vector2f(25.f, 25.f)) == bordered_filler);
		// Outside the inner curve, but inside the outer one.
		CHECK(context->GetElementAtPoint(origin + Vector2f(16.f, 16.f)) == bordered_clip);
		CHECK(context->GetElementAtPoint(origin + Vector2f(5.f, 5.f)) == document);
	}

	SUBCASE("Clipped by a transformed container")
	{
		// The container is drawn 100px to the right of its layout position, and clips its overflowing child there.
		Element* transformed_clip = document->GetElementById("transformed_clip");
		Element* overflowing = document->GetElementById("overflowing");
		const Vector2f origin = transformed_clip->GetAbsoluteOffset(BoxArea::Border);
		CHECK(context->GetElementAtPoint(origin + Vector2f(150.f, 50.f)) == overflowing);
		CHECK(context->GetElementAtPoint(origin + Vector2f(250.f, 50.f)) == document);
		CHECK(context->GetElementAtPoint(origin + Vector2f(50.f, 50.f)) == document);
	}

	SUBCASE("Transformed child of a clipping container")
	{
		// The child is drawn 60px to the right, and the container's scissor region clips it in window coordinates.
		Element* plain_clip = document->GetElementById("plain_clip");
		Element* moved = document->GetElementById("moved");
		const Vector2f origin = plain_clip->GetAbsoluteOffset(BoxArea::Border);
		CHECK(context->GetElementAtPoint(origin + Vector2f(80.f, 50.f)) == moved);
		CHECK(context->GetElementAtPoint(origin + Vector2f(120.f, 50.f)) == document);
		CHECK(context->GetElementAtPoint(origin + Vector2f(30.f, 50.f)) == plain_clip);
	}

	document->Close();
	TestsShell::ShutdownShell();
}
