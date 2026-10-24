"""Device Management Service Conformance Tests."""

from __future__ import annotations

import pytest


def test_get_system_date_and_time(soap_client, schema_devicemgmt):
    """Verify GetSystemDateAndTime returns compliant date structure."""
    response = soap_client.call(
        service="device_service",
        action="http://www.onvif.org/ver10/device/wsdl/GetSystemDateAndTime",
        payload="<tds:GetSystemDateAndTime/>",
        authenticate=False,  # Allowed without auth per ONVIF Core §5.9.1
    )
    assert response.status_code == 200
    schema_devicemgmt.validate(response.text)
    assert "DateTimeType" in response.text
    assert "DaylightSavings" in response.text


def test_get_device_information(soap_client, schema_devicemgmt):
    """Verify GetDeviceInformation returns manufacturer, model, and firmware version."""
    response = soap_client.call(
        service="device_service",
        action="http://www.onvif.org/ver10/device/wsdl/GetDeviceInformation",
        payload="<tds:GetDeviceInformation/>",
        authenticate=True,
    )
    assert response.status_code == 200
    schema_devicemgmt.validate(response.text)
    assert "Manufacturer" in response.text
    assert "Model" in response.text
    assert "FirmwareVersion" in response.text


def test_get_services_sequence(soap_client, schema_devicemgmt):
    """Verify GetServices adheres to strict WSDL element sequence ordering."""
    response = soap_client.call(
        service="device_service",
        action="http://www.onvif.org/ver10/device/wsdl/GetServices",
        payload="<tds:GetServices><tds:IncludeCapability>true</tds:IncludeCapability></tds:GetServices>",
        authenticate=True,
    )
    assert response.status_code == 200
    schema_devicemgmt.validate(response.text)
    assert "Namespace" in response.text
    assert "XAddr" in response.text
    assert "Version" in response.text


def test_get_capabilities(soap_client, schema_devicemgmt):
    """Verify GetCapabilities returns valid capabilities structure."""
    response = soap_client.call(
        service="device_service",
        action="http://www.onvif.org/ver10/device/wsdl/GetCapabilities",
        payload="<tds:GetCapabilities/>",
        authenticate=True,
    )
    assert response.status_code == 200
    schema_devicemgmt.validate(response.text)


def test_invalid_action_fault(soap_client, schema_soap):
    """Verify unsupported operations return SOAP 1.2 ActionNotSupported fault."""
    response = soap_client.call(
        service="device_service",
        action="http://www.onvif.org/ver10/device/wsdl/NonExistentOp",
        payload="<tds:NonExistentOp/>",
        authenticate=True,
    )
    assert response.status_code == 500
    schema_soap.validate(response.text)
    assert "ter:ActionNotSupported" in response.text or "ActionNotSupported" in response.text
