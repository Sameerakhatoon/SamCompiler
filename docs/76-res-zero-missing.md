# ch76 - res = 0 missing

Upstream this chapter fixes a `res = 0;` that was missing from the
`TOKEN_TYPE_IDENTIFIER` arm of `parse_expressionable_single`. Without
it, the parser would think identifier handling had failed and break
out of the expressionable loop early.

SamCompiler already had `res = 0;` here from ch27's first write of
the dispatch (we never duplicated the upstream omission). Marker
chapter only.
