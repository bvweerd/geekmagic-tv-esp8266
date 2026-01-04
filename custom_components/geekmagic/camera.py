"""Deprecated camera platform - replaced by image platform."""

from __future__ import annotations

import logging

from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant
from homeassistant.helpers.entity_platform import AddEntitiesCallback

_LOGGER = logging.getLogger(__name__)


async def async_setup_entry(
    hass: HomeAssistant,
    entry: ConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    """Set up GeekMagic camera from a config entry.

    This is a stub for backward compatibility. The camera platform has been
    replaced by the image platform. Remove and re-add the integration to
    use the new image entity.
    """
    _LOGGER.warning(
        "GeekMagic camera platform is deprecated. "
        "Please remove and re-add the integration to use the new image entity."
    )
    # Don't add any entities - the image platform will handle this now
