"""Sensor platform - re-exports from entities submodule."""

from .entities.sensor import async_setup_entry

__all__ = ["async_setup_entry"]
