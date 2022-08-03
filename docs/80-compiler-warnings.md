# ch80 - fixed compiler warnings

Upstream this chapter drops two `#warning` directives that were
leftover TODOs from earlier chapters. SamCompiler never used those
warning directives; we used `// TODO(chXX)` comments instead and
already cleared them as features landed. Marker chapter.
