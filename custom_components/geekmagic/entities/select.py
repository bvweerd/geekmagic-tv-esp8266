"""Select entities for GeekMagic integration."""

from __future__ import annotations

import logging
from typing import TYPE_CHECKING

from homeassistant.components.select import SelectEntity
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant
from homeassistant.helpers.entity_platform import AddEntitiesCallback

from ..const import DOMAIN
from .base import GeekMagicEntity

if TYPE_CHECKING:
    from ..coordinator import GeekMagicCoordinator

_LOGGER = logging.getLogger(__name__)

# Built-in device modes with their theme numbers
# These are handled by the device firmware, not rendered by the integration
# Theme 3 (Photo Album) is intentionally omitted - used for custom rendered views
BUILTIN_MODES = {
    "Weather Clock Today": 1,
    "Weather Forecast": 2,
    "Time Style 1": 4,
    "Time Style 2": 5,
    "Time Style 3": 6,
    "Simple Weather Clock": 7,
}

# Prefix used to identify custom views in the combined select
CUSTOM_VIEW_PREFIX = ""  # No prefix - views shown by name directly


async def async_setup_entry(
    hass: HomeAssistant,
    entry: ConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    """Set up GeekMagic select entities."""
    coordinator: GeekMagicCoordinator = hass.data[DOMAIN][entry.entry_id]

    entities = [
        GeekMagicDisplaySelect(coordinator),
    ]

    async_add_entities(entities)


class GeekMagicDisplaySelect(GeekMagicEntity, SelectEntity):
    """Unified select entity for choosing what to display.

    Combines device built-in modes (Clock, Weather, System Info) with
    custom views configured in the integration. This provides a single
    control point for users to choose what appears on the display.
    """

    _attr_name = "Display"
    _attr_icon = "mdi:monitor"

    def __init__(self, coordinator: GeekMagicCoordinator) -> None:
        """Initialize display select."""
        super().__init__(coordinator, "display")

    def _get_custom_view_names(self) -> list[str]:
        """Get list of custom view names."""
        store = self.coordinator.get_store()
        if not store:
            return []

        assigned_views = self.coordinator.options.get("assigned_views", [])
        names = []
        for view_id in assigned_views:
            view = store.get_view(view_id)
            if view:
                names.append(view.get("name", view_id))
        return names

    @property
    def options(self) -> list[str]:
        """Return all available display options.

        Built-in modes come first, followed by custom views.
        """
        options = list(BUILTIN_MODES.keys())
        options.extend(self._get_custom_view_names())
        return options if options else ["Clock"]

    @property
    def current_option(self) -> str | None:
        """Return currently selected display option."""
        # Check if coordinator is in builtin mode
        if self.coordinator.display_mode == "builtin":
            theme = self.coordinator.builtin_theme
            for mode_name, mode_theme in BUILTIN_MODES.items():
                if mode_theme == theme:
                    return mode_name
            return "Clock"

        # In custom mode - return current view name
        view_names = self._get_custom_view_names()
        if not view_names:
            # No custom views, default to Clock
            return "Clock"

        current_idx = self.coordinator.current_screen
        if 0 <= current_idx < len(view_names):
            return view_names[current_idx]

        return view_names[0] if view_names else "Clock"

    async def async_select_option(self, option: str) -> None:
        """Handle selection of a display option."""
        if option in BUILTIN_MODES:
            # Built-in mode selected - set device theme and enter builtin mode
            theme = BUILTIN_MODES[option]
            _LOGGER.debug("Switching to built-in mode: %s (theme=%d)", option, theme)
            await self.coordinator.device.set_theme(theme)
            self.coordinator.set_display_mode("builtin", theme)
            # Don't refresh - just poll state to update UI
            await self.coordinator.async_request_refresh()
        else:
            # Custom view selected
            view_names = self._get_custom_view_names()
            if option in view_names:
                view_idx = view_names.index(option)
                _LOGGER.debug("Switching to custom view: %s (index=%d)", option, view_idx)
                self.coordinator.set_display_mode("custom", view_idx)
                # Immediate refresh to show the custom view
                await self.coordinator.async_refresh_display()
