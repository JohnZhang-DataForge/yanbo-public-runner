# Base-game startup services

The service registry now includes `aoc:u` and the `pctl` family, in addition to
`sm:`, `lm`, `apm` and `appletOE`.

The base-game add-on registry is empty. CountAddOnContent (2) validates the PID
request and reports zero installed entries. GetAddOnContentListChangedEvent (8)
returns a real copied event that remains unsignaled while this registry is
unchanged. PrepareAddOnContent (7) reports not found; nonexistent DLC is not
marked prepared or entitled. Listing buffers, installation, hot changes and
purchase-management operations remain unsupported.

The `pctl`, `pctl:s`, `pctl:r` and `pctl:a` endpoints support process service
creation (0/1, with PID input) and client initialization (1). This implements
registration/lifetime only. PIN operations and policy permission queries remain
unsupported and stop explicitly; they do not receive fabricated authorization.
These host services do not access or modify a console/account's policy state.

Native tests cover PID requirements, the empty add-on count, failed preparation,
change-event lifetime, policy object creation and initialization, aliases, and
rejection of unsupported permission requests. Public tests contain no game data.

References (consulted, not copied):

- https://switchbrew.org/wiki/NS_services
- https://github.com/switchbrew/libnx/blob/master/nx/source/services/pctl.c
