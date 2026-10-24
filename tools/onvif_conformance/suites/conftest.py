"""Shared pytest fixtures for ONVIF Profile S Conformance Runner."""

from __future__ import annotations

import base64
from dataclasses import dataclass
from datetime import datetime, timezone
import hashlib
import json
import os
import pathlib
import secrets
import typing
import pytest
import requests
import xmlschema

SCHEMAS_DIR = pathlib.Path(__file__).parent.parent / "schemas"
CONFIG_FILE = pathlib.Path(__file__).parent.parent / "conformance_config.json"


@dataclass
class OnvifTarget:
    """Target configuration for the ONVIF device under test."""

    host: str
    port: int
    admin_username: str
    admin_password: str
    discovery_port: int = 3702


@pytest.fixture(scope="session")
def onvif_target() -> OnvifTarget:
    """Load ONVIF target configuration from environment or config file."""
    host = os.environ.get("ONVIF_TEST_HOST")
    port_env = os.environ.get("ONVIF_TEST_PORT")

    if host and port_env:
        return OnvifTarget(
            host=host,
            port=int(port_env),
            admin_username=os.environ.get("ONVIF_TEST_USER", "admin"),
            admin_password=os.environ.get("ONVIF_TEST_PASS", "AdminPass123!"),
        )

    if CONFIG_FILE.exists():
        with open(CONFIG_FILE, "r", encoding="utf-8") as f:
            data = json.load(f)
            t = data.get("target", {})
            return OnvifTarget(
                host=t.get("host", "127.0.0.1"),
                port=int(t.get("port", 8080)),
                admin_username=t.get("admin_username", "admin"),
                admin_password=t.get("admin_password", "AdminPass123!"),
                discovery_port=int(t.get("discovery_port", 3702)),
            )

    return OnvifTarget(
        host="127.0.0.1",
        port=8080,
        admin_username="admin",
        admin_password="AdminPass123!",
    )


@pytest.fixture(scope="session")
def schema_soap() -> xmlschema.XMLSchema:
    return xmlschema.XMLSchema(SCHEMAS_DIR / "soap-envelope.xsd")


@pytest.fixture(scope="session")
def schema_onvif() -> xmlschema.XMLSchema:
    return xmlschema.XMLSchema(SCHEMAS_DIR / "onvif.xsd")


@pytest.fixture(scope="session")
def schema_devicemgmt() -> xmlschema.XMLSchema:
    return xmlschema.XMLSchema(SCHEMAS_DIR / "devicemgmt.xsd")


@pytest.fixture(scope="session")
def schema_media() -> xmlschema.XMLSchema:
    return xmlschema.XMLSchema(SCHEMAS_DIR / "media.xsd")


@pytest.fixture(scope="session")
def schema_ptz() -> xmlschema.XMLSchema:
    return xmlschema.XMLSchema(SCHEMAS_DIR / "ptz.xsd")


@pytest.fixture(scope="session")
def schema_event() -> xmlschema.XMLSchema:
    return xmlschema.XMLSchema(SCHEMAS_DIR / "event.xsd")


@pytest.fixture(scope="session")
def schema_wsd() -> xmlschema.XMLSchema:
    return xmlschema.XMLSchema(SCHEMAS_DIR / "ws-discovery.xsd")


class SoapTestClient:
    """Helper client to dispatch authenticated SOAP 1.2 requests."""

    def __init__(self, target: OnvifTarget) -> None:
        self.target = target
        self.base_url = f"http://{target.host}:{target.port}/onvif"

    def _create_ws_security_header(self) -> str:
        """Create WS-Security UsernameToken with PasswordDigest per ONVIF Core §5.9.2."""
        raw_nonce = secrets.token_bytes(16)
        nonce_b64 = base64.b64encode(raw_nonce).decode("ascii")
        created = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")

        # Password_Digest = Base64(SHA1(raw_nonce + created + password))
        hasher = hashlib.sha1()
        hasher.update(raw_nonce)
        hasher.update(created.encode("utf-8"))
        hasher.update(self.target.admin_password.encode("utf-8"))
        digest_b64 = base64.b64encode(hasher.digest()).decode("ascii")

        return f"""<wsse:Security xmlns:wsse="http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-secext-1.0.xsd"
                                xmlns:wsu="http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-utility-1.0.xsd">
          <wsse:UsernameToken>
            <wsse:Username>{self.target.admin_username}</wsse:Username>
            <wsse:Password Type="http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-username-token-profile-1.0#PasswordDigest">{digest_b64}</wsse:Password>
            <wsse:Nonce EncodingType="http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-soap-message-security-1.0#Base64Binary">{nonce_b64}</wsse:Nonce>
            <wsu:Created>{created}</wsu:Created>
          </wsse:UsernameToken>
        </wsse:Security>"""

    def call(
        self,
        service: str,
        action: str,
        payload: str,
        authenticate: bool = True,
        timeout: float = 5.0,
    ) -> requests.Response:
        """Send SOAP 1.2 request to /onvif/<service> endpoint."""
        url = f"{self.base_url}/{service}"
        sec_header = self._create_ws_security_header() if authenticate else ""

        envelope = f"""<?xml version="1.0" encoding="UTF-8"?>
<soap:Envelope xmlns:soap="http://www.w3.org/2003/05/soap-envelope"
               xmlns:tds="http://www.onvif.org/ver10/device/wsdl"
               xmlns:trt="http://www.onvif.org/ver10/media/wsdl"
               xmlns:tptz="http://www.onvif.org/ver10/ptz/wsdl"
               xmlns:tev="http://www.onvif.org/ver10/events/wsdl"
               xmlns:tt="http://www.onvif.org/ver10/schema"
               xmlns:wsa="http://www.w3.org/2005/08/addressing">
  <soap:Header>
    <wsa:Action>{action}</wsa:Action>
    {sec_header}
  </soap:Header>
  <soap:Body>
    {payload}
  </soap:Body>
</soap:Envelope>"""

        headers = {
            "Content-Type": f'application/soap+xml; charset=utf-8; action="{action}"',
        }
        return requests.post(url, data=envelope.encode("utf-8"), headers=headers, timeout=timeout)

    def call_raw(
        self,
        url: str,
        action: str,
        payload: str,
        authenticate: bool = True,
        timeout: float = 5.0,
    ) -> requests.Response:
        """Send SOAP 1.2 request to an arbitrary URL (e.g. subscription XAddr)."""
        sec_header = self._create_ws_security_header() if authenticate else ""

        envelope = f"""<?xml version="1.0" encoding="UTF-8"?>
<soap:Envelope xmlns:soap="http://www.w3.org/2003/05/soap-envelope"
               xmlns:wsnt="http://docs.oasis-open.org/wsn/b-2"
               xmlns:tev="http://www.onvif.org/ver10/events/wsdl"
               xmlns:wsa="http://www.w3.org/2005/08/addressing">
  <soap:Header>
    <wsa:Action>{action}</wsa:Action>
    {sec_header}
  </soap:Header>
  <soap:Body>
    {payload}
  </soap:Body>
</soap:Envelope>"""

        headers = {
            "Content-Type": f'application/soap+xml; charset=utf-8; action="{action}"',
        }
        return requests.post(url, data=envelope.encode("utf-8"), headers=headers, timeout=timeout)


@pytest.fixture
def soap_client(onvif_target: OnvifTarget) -> SoapTestClient:
    return SoapTestClient(onvif_target)
