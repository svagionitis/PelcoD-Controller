"""Media Service Profile S Conformance Tests."""

from __future__ import annotations

import xml.etree.ElementTree as ET
import pytest
import requests


def test_profile_s_invariants(soap_client, schema_media):
    """Verify Profile S mandatory invariant: each profile pairs a VSC with a VEC."""
    response = soap_client.call(
        service="media_service",
        action="http://www.onvif.org/ver10/media/wsdl/GetProfiles",
        payload="<trt:GetProfiles/>",
        authenticate=True,
    )
    assert response.status_code == 200
    schema_media.validate(response.text)

    root = ET.fromstring(response.text)
    profiles = root.findall(".//{http://www.onvif.org/ver10/media/wsdl}Profiles")
    assert len(profiles) >= 1, "Profile S device must expose at least one media profile"

    for prof in profiles:
        token = prof.attrib.get("token")
        vsc = prof.find("{http://www.onvif.org/ver10/schema}VideoSourceConfiguration")
        vec = prof.find("{http://www.onvif.org/ver10/schema}VideoEncoderConfiguration")
        assert vsc is not None, f"Profile {token} missing VideoSourceConfiguration"
        assert vec is not None, f"Profile {token} missing VideoEncoderConfiguration"


def test_get_stream_uri_rtsp(soap_client, schema_media):
    """Verify GetStreamUri returns valid RTSP URI."""
    response = soap_client.call(
        service="media_service",
        action="http://www.onvif.org/ver10/media/wsdl/GetStreamUri",
        payload="""<trt:GetStreamUri>
          <trt:StreamSetup>
            <tt:Stream>RTP-Unicast</tt:Stream>
            <tt:Transport><tt:Protocol>RTSP</tt:Protocol></tt:Transport>
          </trt:StreamSetup>
          <trt:ProfileToken>Profile1</trt:ProfileToken>
        </trt:GetStreamUri>""",
        authenticate=True,
    )
    assert response.status_code == 200
    schema_media.validate(response.text)
    assert "rtsp://" in response.text


def test_get_snapshot_uri(soap_client, schema_media):
    """Verify GetSnapshotUri returns valid URI pointing to accessible JPEG snapshot."""
    response = soap_client.call(
        service="media_service",
        action="http://www.onvif.org/ver10/media/wsdl/GetSnapshotUri",
        payload="<trt:GetSnapshotUri><trt:ProfileToken>Profile1</trt:ProfileToken></trt:GetSnapshotUri>",
        authenticate=True,
    )
    assert response.status_code == 200
    schema_media.validate(response.text)

    root = ET.fromstring(response.text)
    uri_node = root.find(".//{http://www.onvif.org/ver10/schema}Uri")
    assert uri_node is not None and uri_node.text

    # Snapshot retrieval check
    snap_resp = requests.get(uri_node.text, timeout=3.0)
    assert snap_resp.status_code == 200
    assert "image/jpeg" in snap_resp.headers.get("Content-Type", "")
    assert len(snap_resp.content) > 50
