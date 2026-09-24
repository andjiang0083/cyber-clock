<!--
Thanks for the PR. Keep the description short and factual: what changed, why, and
what you actually verified on hardware. 中文也可以。
-->

## What this changes

<!-- One or two sentences. Link the issue it closes, if any: "Closes #12". -->

## Why

<!-- The behaviour that motivated it. If it is a bug fix, what was the symptom? -->

## Which targets?

- [ ] StickS3 (`src/`)
- [ ] Cardputer (`cardputer/src/`)
- [ ] Shared behaviour changed — **both copies were updated** and both build

## Verification

```
Both targets build (`pio run` at the root, then inside cardputer/):  [ ] yes
Flashed and verified on:   [ ] StickS3   [ ] Cardputer   [ ] neither (build-only)
Checked after the change:  [ ] 3 display modes  [ ] 6 themes  [ ] 6 rain sets
                           [ ] brightness  [ ] power save entry/exit  [ ] USB override
```

<!--
If the change is visual, attach a photo or a screenshot. If it touches the power
save path, say explicitly whether the StickS3 still reaches Light Sleep.
-->

## Notes for the reviewer

<!-- Anything you are unsure about, anything you deliberately did not do, or a
     smaller follow-up that should be a separate PR. -->
