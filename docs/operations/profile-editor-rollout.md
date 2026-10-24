# Profile editor rollout

The public editor currently enables only **اطلاعات اصلی و معرفی**. Its sidebar
contains seven items: the active basic section, followed by six disabled sections
with lock icons. There is no scrolling inside this menu. Restoring an older
selected section does not reopen an advanced form, and the section picker is
unavailable during this rollout.

The notice below the menu explains that additional sections will become available
gradually. Its notification button remains disabled until a notification signup
service exists; it does not claim to register a subscription.

To enable the existing advanced editor, an operator can explicitly set
`PROFILE_EDITOR_ADVANCED_SECTIONS=true` on the application service and restart it.
An absent variable or any other value keeps the basic rollout enabled. This
switch changes editor navigation; stored sections, public rendering, and content
APIs retain their existing behavior.

The isolated `docker-compose.profile-test.yml` service opts into the complete
editor so the advanced API and browser suites continue to exercise existing
forms. The dashboard browser test explicitly uses the basic rollout and checks
the seven menu items, disabled locks, stale selection recovery, preservation of
stored advanced data, and the absence of menu scrolling at seven viewport widths.
