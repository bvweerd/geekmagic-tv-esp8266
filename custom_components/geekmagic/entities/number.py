"""Number entities for GeekMagic integration."""

from __future__ import annotations

import logging
from typing import TYPE_CHECKING

from homeassistant.components.number import NumberEntity, NumberMode
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant
from homeassistant.helpers.entity_platform import AddEntitiesCallback

from ..const import DOMAIN
from .base import GeekMagicEntity

if TYPE_CHECKING:
    from ..coordinator import GeekMagicCoordinator

_LOGGER = logging.getLogger(__name__)


async def async_setup_entry(
    hass: HomeAssistant,
    entry: ConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    """Set up GeekMagic number entities."""
    coordinator: GeekMagicCoordinator = hass.data[DOMAIN][entry.entry_id]

    entities = [
        GeekMagicBrightnessNumber(coordinator),
        GeekMagicRefreshIntervalNumber(coordinator),
        GeekMagicCycleIntervalNumber(coordinator),
    ]

    async_add_entities(entities)


class GeekMagicBrightnessNumber(GeekMagicEntity, NumberEntity):
    """Number entity for display brightness."""

    _attr_name = "Brightness"
    _attr_icon = "mdi:brightness-6"
    _attr_native_min_value = 0
    _attr_native_max_value = 100
    _attr_native_step = 1
    _attr_native_unit_of_measurement = "%"
    _attr_mode = NumberMode.SLIDER

    def __init__(self, coordinator: GeekMagicCoordinator) -> None:
        """Initialize brightness number."""
        super().__init__(coordinator, "brightness")

    @property
    def native_value(self) -> float | None:
        """Return current brightness from device."""
        return self.coordinator.device_brightness

    async def async_set_native_value(self, value: float) -> None:
        """Set brightness."""
        brightness = int(value)
        await self.coordinator.device.set_brightness(brightness)
        # Update local cache immediately so UI reflects change
        self.coordinator.device_brightness = brightness
        self.async_write_ha_state()


class GeekMagicRefreshIntervalNumber(GeekMagicEntity, NumberEntity):
    """Number entity for refresh interval."""

    _attr_name = "Refresh Interval"
    _attr_icon = "mdi:timer-refresh"
    _attr_native_min_value = 5
    _attr_native_max_value = 300
    _attr_native_step = 5
    _attr_native_unit_of_measurement = "s"
    _attr_mode = NumberMode.BOX

    def __init__(self, coordinator: GeekMagicCoordinator) -> None:
        """Initialize refresh interval number."""
        super().__init__(coordinator, "refresh_interval")

    @property
    def native_value(self) -> float | None:
        """Return current refresh interval."""
        return self.coordinator.options.get("refresh_interval", 30)

    async def async_set_native_value(self, value: float) -> None:
        """Set refresh interval."""
        # Update options
        new_options = {**self.coordinator.entry.options, "refresh_interval": int(value)}
        self.hass.config_entries.async_update_entry(self.coordinator.entry, options=new_options)


class GeekMagicCycleIntervalNumber(GeekMagicEntity, NumberEntity):
    """Number entity for view cycle interval.

    Controls how often the display cycles between custom views.
    Set to 0 to disable automatic cycling (manual view selection only).
    """

    _attr_name = "View Cycle Interval"
    _attr_icon = "mdi:view-carousel"
    _attr_native_min_value = 0
    _attr_native_max_value = 300
    _attr_native_step = 5
    _attr_native_unit_of_measurement = "s"
    _attr_mode = NumberMode.BOX

    def __init__(self, coordinator: GeekMagicCoordinator) -> None:
        """Initialize cycle interval number."""
        super().__init__(coordinator, "cycle_interval")

    @property
    def native_value(self) -> float | None:
        """Return current cycle interval (0 = disabled)."""
        return self.coordinator.options.get("screen_cycle_interval", 0)

    async def async_set_native_value(self, value: float) -> None:
        """Set cycle interval."""
        # Update options
        new_options = {
            **self.coordinator.entry.options,
            "screen_cycle_interval": int(value),
        }
        self.hass.config_entries.async_update_entry(self.coordinator.entry, options=new_options)
