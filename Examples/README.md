# Host Examples

Host-specific runners and backend examples live here, outside the portable VM
library. Production firmware can provide its own callbacks and task/thread entry
points without pulling platform glue into `MVM/`.

Start with `MVM/INTEGRATION.md` for the parent-project checklist and use
`MVM_BareMetalPort/` as the no-SDL reference implementation.
