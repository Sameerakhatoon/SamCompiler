# ch69 - project cleanup

Upstream this chapter is purely cosmetic: blank lines, a couple of
forward declarations the book hadn't gotten around to adding earlier,
and removing a `#warning "Remember to calculate scope offsets..."` line
that was outdated by ch58/61.

In SamCompiler:

- The forward decls (`parse_body`, `parse_keyword`) are already in the
  prototype block from earlier chapters.
- I never had the `#warning` text - I used a `// TODO(ch43+)` comment
  that I cleared in ch58 when the offset code actually landed.

So nothing to do. Marker chapter only; tests stay at 44 passing.
