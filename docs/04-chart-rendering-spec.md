# Chart Rendering Specification

## Status
- Implemented in the current repo: natal, synastry, composite, and transit rendering; shared scene generation; SVG export; PNG rasterization; and the four built-in theme presets.
- Partially implemented: some collision-management, warning-layout, and export-profile language here is stricter than the current automated guarantees.
- Planned/future-facing: advanced filtering and stronger layout guarantees remain rendering goals rather than fully enforced invariants.

## Rendering Mission
Asteria rendering is a product-defining subsystem. It must produce:
- elegant on-screen charts
- deterministic SVG exports
- high-quality PNG exports
- readable chart layouts for power astrologers
- visually distinct textbook and luxury themes

## Rendering Principles
- Vector-first, always
- Shared geometry model across screen and export
- Clean label discipline
- Themeable without layout chaos
- Deterministic output for the same chart request and theme

## Rendering Pipeline

### Stage 1: Input Normalization
Input is a fully normalized ComputedChart and a ChartTheme.

### Stage 2: Layout Model
Produce a layout model with:
- outer wheel geometry
- sign ring
- house ring
- object placement ring
- aspect layer
- center metadata block
- comparison overlays for synastry
- transit overlays for transit-to-natal

### Stage 3: Scene Graph
Convert layout to a vector scene graph:
- circles
- arcs
- paths
- text runs
- glyph symbols
- line segments
- markers
- clip regions
- named layers

### Stage 4: Render Targets
- Screen renderer
- SVG serializer
- PNG rasterizer

## Supported Chart Layouts in V1

### Natal
- Single wheel
- Sign ring
- House ring
- Object markers
- Aspect lines
- Optional compact metadata block
- Warning banner area if time uncertainty exists

### Synastry
- Bi-wheel
- Inner wheel: primary
- Outer wheel: secondary
- Aspect lines between charts
- Optional relationship metadata summary

### Composite
- Single-wheel composite
- Explicit composite badge
- Reduced metadata noise

### Transit-to-Natal
- Natal inner wheel
- Transit outer wheel
- Distinct transit coloring
- Today/date/time context label

## Theme System
Theme controls must include:
- glyph style
- line weights
- aspect colors
- typography
- theme presets

### V1 Theme Presets
1. Textbook Light
2. Textbook Monochrome
3. Luxury Light
4. Luxury Dark

## Typography Rules
- Avoid decorative overload
- Use highly legible sign/object labeling
- Support export-safe font fallback strategy
- Ensure labels remain clear when rasterized

## Labeling and Collision Rules
- Never allow object labels to overlap in export output
- Prefer radial displacement before leader lines
- Use truncation only as last resort
- Ensure uncertainty warnings never collide with central metadata

## Warning Visualization
When birth time is uncertain:
- visible warning marker in chart frame
- no hidden assumptions
- if noon default used, badge must state that explicitly
- if no houses are available, suppress house ring and replace with explicit “No Houses / Unknown Birth Time” treatment

## Aspect Layer Rules
- Color by aspect family via theme palette
- Keep line thickness restrained
- Preserve readability in dense synastry charts
- Support optional simplified aspect filtering later, but not required for V1

## SVG Export Requirements
- SVG must be deterministic
- Layers and element IDs should be stable where practical
- Text may remain as text where font strategy permits
- Optional future path outlining is allowed but not required in V1

## PNG Export Requirements
- Rasterized from the same scene graph
- Export profiles:
  - screen share
  - print preview
  - high resolution publication

## Performance Expectations
- Natal render should feel immediate
- Synastry render should remain smooth for common cases
- Export should be deterministic and reliable even if slower than screen rendering
