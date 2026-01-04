"""Fullscreen layout for GeekMagic displays."""

from __future__ import annotations

from .base import Layout, Slot


class FullscreenLayout(Layout):
    """Single widget taking full 240x240 display with no padding.

    +---------------------+
    |                     |
    |                     |
    |      FULLSCREEN     |
    |       (slot 0)      |
    |                     |
    |                     |
    +---------------------+
    """

    def __init__(self, padding: int = 0, gap: int = 0) -> None:
        """Initialize fullscreen layout.

        Args:
            padding: Ignored, always 0 for edge-to-edge display
            gap: Ignored, only one slot
        """
        # Force 0 padding for true edge-to-edge display
        super().__init__(padding=0, gap=0)

    def _calculate_slots(self) -> None:
        """Calculate single fullscreen slot."""
        self.slots = [
            Slot(
                index=0,
                rect=(0, 0, self.width, self.height),
            )
        ]
