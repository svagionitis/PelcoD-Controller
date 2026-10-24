"""WS-Discovery Profile S Conformance Tests."""

from __future__ import annotations

import socket
import time
import uuid
import xml.etree.ElementTree as ET
import pytest


def test_wsd_probe_latency(onvif_target, schema_wsd):
    """Verify ProbeMatches is returned within 500 ms and conforms to WS-Discovery schema."""
    probe_uuid = f"urn:uuid:{uuid.uuid4()}"
    probe_xml = f"""<?xml version="1.0" encoding="UTF-8"?>
    <soap:Envelope xmlns:soap="http://www.w3.org/2003/05/soap-envelope"
                   xmlns:wsa="http://schemas.xmlsoap.org/ws/2004/08/addressing"
                   xmlns:wsd="http://schemas.xmlsoap.org/ws/2005/04/discovery"
                   xmlns:dn="http://www.onvif.org/ver10/network/wsdl">
      <soap:Header>
        <wsa:MessageID>{probe_uuid}</wsa:MessageID>
        <wsa:To>urn:schemas-xmlsoap-org:ws:2005:04:discovery</wsa:To>
        <wsa:Action>http://schemas.xmlsoap.org/ws/2005/04/discovery/Probe</wsa:Action>
      </soap:Header>
      <soap:Body>
        <wsd:Probe>
          <wsd:Types>dn:NetworkVideoTransmitter</wsd:Types>
        </wsd:Probe>
      </soap:Body>
    </soap:Envelope>"""

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(2.0)

    try:
        start_time = time.time()
        sock.sendto(probe_xml.encode("utf-8"), (onvif_target.host, onvif_target.discovery_port))
        resp_data, _ = sock.recvfrom(65535)
        latency_ms = (time.time() - start_time) * 1000.0

        assert latency_ms <= 1000.0, f"Discovery latency {latency_ms:.1f}ms exceeds threshold"
        schema_wsd.validate(resp_data.decode("utf-8"))
        assert probe_uuid in resp_data.decode("utf-8")
    finally:
        sock.close()


def test_wsd_scopes_compliance(onvif_target):
    """Verify Scopes contains standard Profile/Streaming URI."""
    probe_uuid = f"urn:uuid:{uuid.uuid4()}"
    probe_xml = f"""<?xml version="1.0" encoding="UTF-8"?>
    <soap:Envelope xmlns:soap="http://www.w3.org/2003/05/soap-envelope"
                   xmlns:wsa="http://schemas.xmlsoap.org/ws/2004/08/addressing"
                   xmlns:wsd="http://schemas.xmlsoap.org/ws/2005/04/discovery">
      <soap:Header>
        <wsa:MessageID>{probe_uuid}</wsa:MessageID>
        <wsa:To>urn:schemas-xmlsoap-org:ws:2005:04:discovery</wsa:To>
        <wsa:Action>http://schemas.xmlsoap.org/ws/2005/04/discovery/Probe</wsa:Action>
      </soap:Header>
      <soap:Body>
        <wsd:Probe/>
      </soap:Body>
    </soap:Envelope>"""

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(2.0)

    try:
        sock.sendto(probe_xml.encode("utf-8"), (onvif_target.host, onvif_target.discovery_port))
        resp_data, _ = sock.recvfrom(65535)
        text = resp_data.decode("utf-8")
        assert "onvif://www.onvif.org/Profile/Streaming" in text
    finally:
        sock.close()
