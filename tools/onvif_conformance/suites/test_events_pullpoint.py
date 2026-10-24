"""Event Service PullPoint Conformance Tests."""

from __future__ import annotations

import xml.etree.ElementTree as ET
import pytest


def test_pullpoint_lifecycle(soap_client, schema_event):
    """Verify CreatePullPointSubscription, PullMessages, Renew, and Unsubscribe lifecycle."""
    # 1. CreatePullPointSubscription
    create_resp = soap_client.call(
        service="event_service",
        action="http://www.onvif.org/ver10/events/wsdl/CreatePullPointSubscription",
        payload="<tev:CreatePullPointSubscription><tev:InitialTerminationTime>PT60S</tev:InitialTerminationTime></tev:CreatePullPointSubscription>",
        authenticate=True,
    )
    assert create_resp.status_code == 200
    schema_event.validate(create_resp.text)

    root = ET.fromstring(create_resp.text)
    addr_node = root.find(".//{http://www.w3.org/2005/08/addressing}Address")
    assert addr_node is not None and addr_node.text
    subscription_url = addr_node.text

    # 2. PullMessages
    pull_resp = soap_client.call_raw(
        url=subscription_url,
        action="http://www.onvif.org/ver10/events/wsdl/PullPointSubscription/PullMessagesRequest",
        payload="<tev:PullMessages><tev:Timeout>PT1S</tev:Timeout><tev:MessageLimit>5</tev:MessageLimit></tev:PullMessages>",
        authenticate=True,
    )
    assert pull_resp.status_code == 200
    schema_event.validate(pull_resp.text)

    # 3. Renew
    renew_resp = soap_client.call_raw(
        url=subscription_url,
        action="http://docs.oasis-open.org/wsrf/rw-2/Renew",
        payload="<wsnt:Renew><wsnt:TerminationTime>PT120S</wsnt:TerminationTime></wsnt:Renew>",
        authenticate=True,
    )
    assert renew_resp.status_code == 200

    # 4. Unsubscribe
    unsub_resp = soap_client.call_raw(
        url=subscription_url,
        action="http://docs.oasis-open.org/wsrf/rw-2/Unsubscribe",
        payload="<wsnt:Unsubscribe/>",
        authenticate=True,
    )
    assert unsub_resp.status_code == 200
