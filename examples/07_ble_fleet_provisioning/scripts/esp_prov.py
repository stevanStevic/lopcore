#!/usr/bin/env python
#
# SPDX-FileCopyrightText: 2018-2022 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
#

import argparse
import asyncio
import json
import os
import struct
import sys
import textwrap
import time
from getpass import getpass
import boto3
import requests

# URLs for Amazon Root CA certificates
AMAZON_ROOT_CA_URL = "https://www.amazontrust.com/repository/AmazonRootCA1.pem"

def download_root_ca():
    try:
        # Download the Amazon Root CA certificate
        response = requests.get(AMAZON_ROOT_CA_URL)
        response.raise_for_status()
        return response.text
    except requests.exceptions.RequestException as e:
        print(f"Error downloading Amazon Root CA certificate: {e}")
        return None

def create_provisioning_claim(template_name, aws_region):
    # Create a Boto3 client for the AWS IoT service
    client = boto3.client('iot', region_name=aws_region)

    try:
        # Call the create_provisioning_claim API
        response = client.create_provisioning_claim(
            templateName=template_name
        )

        # Extract the certificate and private key from the response
        cert_pem = response['certificatePem']
        private_key_pem = response['keyPair']['PrivateKey']

        # Create a new JSON with the provisioing template, certificate and private key
        new_json = {
            "provisioning_template": template_name,
            "certificate": cert_pem,
            "private_key": private_key_pem
        }

        return new_json

    except Exception as e:
        print(f"Error creating provisioning claim: {e}")
        return None

# ESP32 provisioning-related code
try:
    import prov
    import security
    import transport

except ImportError:
    idf_path = os.environ['IDF_PATH']
    sys.path.insert(0, idf_path + '/components/protocomm/python')
    sys.path.insert(1, idf_path + '/tools/esp_prov')

    import prov
    import security
    import transport

# Set this to true to allow exceptions to be thrown
config_throw_except = False


def on_except(err):
    if config_throw_except:
        raise RuntimeError(err)
    else:
        print(err)


def get_security(secver, username, password, pop='', verbose=False):
    if secver == 2:
        return security.Security2(username, password, verbose)
    elif secver == 1:
        return security.Security1(pop, verbose)
    elif secver == 0:
        return security.Security0(verbose)
    return None


async def get_transport(sel_transport, service_name):
    try:
        tp = None
        if (sel_transport in ['softap', 'httpd']):
            if service_name is None:
                service_name = '192.168.4.1:80'
            tp = transport.Transport_HTTP(service_name)
        elif (sel_transport == 'ble'):
            nu_lookup = {'prov-session': 'ff51', 'prov-config': 'ff52', 'proto-ver': 'ff53'}
            tp = transport.Transport_BLE(service_uuid='021a9004-0382-4aea-bff4-6b3f1c5adfb4',
                                         nu_lookup=nu_lookup)
            await tp.connect(devname=service_name)
        elif (sel_transport == 'console'):
            tp = transport.Transport_Console()
        return tp
    except RuntimeError as e:
        on_except(e)
        return None


async def version_match(tp, protover, verbose=False):
    try:
        response = await tp.send_data('proto-ver', protover)

        if verbose:
            print('proto-ver response : ', response)

        if response.lower() == protover.lower():
            return True

        try:
            info = json.loads(response)
            if info['prov']['ver'].lower() == protover.lower():
                return True

        except ValueError:
            return False

    except Exception as e:
        on_except(e)
        return None


async def has_capability(tp, capability='none', verbose=False):
    try:
        response = await tp.send_data('proto-ver', capability)

        if verbose:
            print('proto-ver response : ', response)

        try:
            info = json.loads(response)
            supported_capabilities = info['prov']['cap']
            if capability.lower() == 'none':
                return True
            elif capability in supported_capabilities:
                return True
            return False

        except ValueError:
            return False

    except RuntimeError as e:
        on_except(e)

    return False


async def get_version(tp):
    response = None
    try:
        response = await tp.send_data('proto-ver', '---')
    except RuntimeError as e:
        on_except(e)
        response = ''
    return response


async def establish_session(tp, sec):
    try:
        response = None
        while True:
            request = sec.security_session(response)
            if request is None:
                break
            response = await tp.send_data('prov-session', request)
            if (response is None):
                return False
        return True
    except RuntimeError as e:
        on_except(e)
        return None


async def custom_config(tp, sec, custom_info, custom_ver):
    try:
        message = prov.custom_config_request(sec, custom_info, custom_ver)
        response = await tp.send_data('custom-config', message)
        return (prov.custom_config_response(sec, response) == 0)
    except RuntimeError as e:
        on_except(e)
        return None


async def custom_data(tp, sec, custom_data, chunk_size=256):
    try:
        # BleDataFramer protocol: 4-byte little-endian length prefix, then raw JSON payload.
        json_bytes = json.dumps(custom_data).encode('utf-8')
        framed = struct.pack('<I', len(json_bytes)) + json_bytes

        chunks = [framed[i:i + chunk_size] for i in range(0, len(framed), chunk_size)]

        for idx, chunk in enumerate(chunks):
            message = prov.custom_data_request(sec, chunk.decode('latin-1'))
            response = await tp.send_data('aws-data', message)

            # Intermediate chunks may produce an empty / null response on the device
            # (handler has not yet received the full frame).  Treat that as success.
            is_last = idx == len(chunks) - 1
            status = prov.custom_data_response(sec, response)
            if is_last and status != 0:
                print(f"Failed to send final chunk (status={status})")
                return False

        print(f"All {len(chunks)} chunks sent successfully")
        return True
    except RuntimeError as e:
        on_except(e)
        return None


async def scan_wifi_APs(sel_transport, tp, sec):
    APs = []
    group_channels = 0
    readlen = 100
    if sel_transport in ['softap', 'httpd']:
        group_channels = 5
    elif sel_transport == 'ble':
        readlen = 4
    try:
        message = prov.scan_start_request(sec, blocking=True, group_channels=group_channels)
        start_time = time.time()
        response = await tp.send_data('prov-scan', message)
        stop_time = time.time()
        print('++++ Scan process executed in ' + str(stop_time - start_time) + ' sec')
        prov.scan_start_response(sec, response)

        message = prov.scan_status_request(sec)
        response = await tp.send_data('prov-scan', message)
        result = prov.scan_status_response(sec, response)
        print('++++ Scan results : ' + str(result['count']))
        if result['count'] != 0:
            index = 0
            remaining = result['count']
            while remaining:
                count = [remaining, readlen][remaining > readlen]
                message = prov.scan_result_request(sec, index, count)
                response = await tp.send_data('prov-scan', message)
                APs += prov.scan_result_response(sec, response)
                remaining -= count
                index += count

    except RuntimeError as e:
        on_except(e)
        return None

    return APs


async def send_wifi_config(tp, sec, ssid, passphrase):
    try:
        message = prov.config_set_config_request(sec, ssid, passphrase)
        response = await tp.send_data('prov-config', message)
        return (prov.config_set_config_response(sec, response) == 0)
    except RuntimeError as e:
        on_except(e)
        return None


async def apply_wifi_config(tp, sec):
    try:
        message = prov.config_apply_config_request(sec)
        response = await tp.send_data('prov-config', message)
        return (prov.config_apply_config_response(sec, response) == 0)
    except RuntimeError as e:
        on_except(e)
        return None


async def get_wifi_config(tp, sec):
    try:
        message = prov.config_get_status_request(sec)
        response = await tp.send_data('prov-config', message)
        return prov.config_get_status_response(sec, response)
    except EOFError:
        print('---- BLE connection lost (device may have restarted) ----')
        return 'disconnected'
    except Exception as e:
        if 'BleakDBusError' in type(e).__name__ or 'BleakError' in type(e).__name__:
            print('---- BLE disconnected: %s ----' % str(e))
            return 'disconnected'
        on_except(e)
        return None


async def wait_wifi_connected(tp, sec):
    TIME_PER_POLL = 5
    retry = 3

    while True:
        time.sleep(TIME_PER_POLL)
        print('\n==== Wi-Fi connection state  ====')
        ret = await get_wifi_config(tp, sec)
        if ret == 'connecting':
            continue
        elif ret == 'connected':
            print('==== Provisioning was successful ====')
            return True
        elif ret == 'disconnected':
            print('---- Device disconnected (provisioning failed or device restarted) ----')
            return False
        elif retry > 0:
            retry -= 1
            print('Waiting to poll status again (status %s, %d tries left)...' % (ret, retry))
        else:
            print('---- Provisioning failed! ----')
            return False


async def reset_wifi(tp, sec):
    try:
        message = prov.ctrl_reset_request(sec)
        response = await tp.send_data('prov-ctrl', message)
        prov.ctrl_reset_response(sec, response)

    except RuntimeError as e:
        on_except(e)
        return None


async def reprov_wifi(tp, sec):
    try:
        message = prov.ctrl_reprov_request(sec)
        response = await tp.send_data('prov-ctrl', message)
        prov.ctrl_reprov_response(sec, response)

    except RuntimeError as e:
        on_except(e)
        return None


def desc_format(*args):
    desc = ''
    for arg in args:
        desc += textwrap.fill(replace_whitespace=False, text=arg) + '\n'
    return desc


async def main():
    parser = argparse.ArgumentParser(description=desc_format(
                                     'ESP Provisioning tool for configuring devices '
                                     'running protocomm based provisioning service.',
                                     'See esp-idf/examples/provisioning for sample applications'),
                                     formatter_class=argparse.RawTextHelpFormatter)

    parser.add_argument('--transport', required=True, dest='mode', type=str,
                        help=desc_format(
                            'Mode of transport over which provisioning is to be performed.',
                            'This should be one of "softap", "ble", "console" (or "httpd" which is an alias of softap)'))

    parser.add_argument('--service_name', dest='name', type=str,
                        help=desc_format(
                            'This specifies the name of the provisioning service to connect to, '
                            'depending upon the mode of transport :',
                            '\t- transport "ble"          : The BLE Device Name',
                            '\t- transport "softap/httpd" : HTTP Server hostname or IP',
                            '\t                       (default "192.168.4.1:80")'))

    parser.add_argument('--proto_ver', dest='version', type=str, default='',
                        help=desc_format(
                            'This checks the protocol version of the provisioning service running '
                            'on the device before initiating Wi-Fi configuration'))

    parser.add_argument('--sec_ver', dest='secver', type=int, default=None,
                        help=desc_format(
                            'Protocomm security scheme used by the provisioning service for secure '
                            'session establishment. Accepted values are :',
                            '\t- 0 : No security',
                            '\t- 1 : X25519 key exchange + AES-CTR encryption',
                            '\t      + Authentication using Proof of Possession (PoP)',
                            '\t- 2 : SRP6a + AES-GCM encryption',
                            'In case device side application uses IDF\'s provisioning manager, '
                            'the compatible security version is automatically determined from '
                            'capabilities retrieved via the version endpoint'))

    parser.add_argument('--pop', dest='sec1_pop', type=str, default='',
                        help=desc_format(
                            'This specifies the Proof of possession (PoP) when security scheme 1 '
                            'is used'))

    parser.add_argument('--sec2_username', dest='sec2_usr', type=str, default='',
                        help=desc_format(
                            'Username for security scheme 2 (SRP6a)'))

    parser.add_argument('--sec2_pwd', dest='sec2_pwd', type=str, default='',
                        help=desc_format(
                            'Password for security scheme 2 (SRP6a)'))

    parser.add_argument('--sec2_gen_cred', help='Generate salt and verifier for security scheme 2 (SRP6a)', action='store_true')

    parser.add_argument('--sec2_salt_len', dest='sec2_salt_len', type=int, default=16,
                        help=desc_format(
                            'Salt length for security scheme 2 (SRP6a)'))

    parser.add_argument('--ssid', dest='ssid', type=str, default='',
                        help=desc_format(
                            'This configures the device to use SSID of the Wi-Fi network to which '
                            'we would like it to connect to permanently, once provisioning is complete. '
                            'If Wi-Fi scanning is supported by the provisioning service, this need not '
                            'be specified'))

    parser.add_argument('--passphrase', dest='passphrase', type=str,
                        help=desc_format(
                            'This configures the device to use Passphrase for the Wi-Fi network to which '
                            'we would like it to connect to permanently, once provisioning is complete. '
                            'If Wi-Fi scanning is supported by the provisioning service, this need not '
                            'be specified'))

    parser.add_argument('--reset', help='Reset WiFi', action='store_true')

    parser.add_argument('--reprov', help='Reprovision WiFi', action='store_true')

    parser.add_argument('--template_name', dest='template_name', type=str, required=True,
                        help=desc_format(
                            'Name of the AWS IoT provisioning template to use for claiming.'))

    parser.add_argument('--aws_endpoint', dest='aws_endpoint', type=str, required=True,
                        help=desc_format(
                            'AWS IoT endpoint to use.'))

    parser.add_argument('--aws_region', dest='aws_region', type=str, required=True,
                        help=desc_format(
                            'AWS region (e.g. eu-west-1) used for boto3 IoT client.'))

    parser.add_argument('-v','--verbose', help='Increase output verbosity', action='store_true')

    args = parser.parse_args()

    if args.secver == 2 and args.sec2_gen_cred:
        if not args.sec2_usr or not args.sec2_pwd:
            raise ValueError('Username/password cannot be empty for security scheme 2 (SRP6a)')

        print('==== Salt-verifier for security scheme 2 (SRP6a) ====')
        security.sec2_gen_salt_verifier(args.sec2_usr, args.sec2_pwd, args.sec2_salt_len)
        sys.exit()

    obj_transport = await get_transport(args.mode.lower(), args.name)
    if obj_transport is None:
        raise RuntimeError('Failed to establish connection')

    try:
        # If security version not specified check in capabilities
        if args.secver is None:
            if not await has_capability(obj_transport):
                print('Security capabilities could not be determined, please specify "--sec_ver" explicitly')
                raise ValueError('Invalid Security Version')

            args.secver = int(not await has_capability(obj_transport, 'no_sec'))
            print(f'==== Security Scheme: {args.secver} ====')

        if (args.secver == 1):
            if not await has_capability(obj_transport, 'no_pop'):
                if len(args.sec1_pop) == 0:
                    prompt_str = 'Proof of Possession required: '
                    args.sec1_pop = getpass(prompt_str)
            elif len(args.sec1_pop) != 0:
                print('Proof of Possession will be ignored')
                args.sec1_pop = ''

        if (args.secver == 2):
            if len(args.sec2_usr) == 0:
                args.sec2_usr = input('Security Scheme 2 - SRP6a Username required: ')
            if len(args.sec2_pwd) == 0:
                prompt_str = 'Security Scheme 2 - SRP6a Password required: '
                args.sec2_pwd = getpass(prompt_str)

        obj_security = get_security(args.secver, args.sec2_usr, args.sec2_pwd, args.sec1_pop, args.verbose)
        if obj_security is None:
            raise ValueError('Invalid Security Version')

        if args.version != '':
            print('\n==== Verifying protocol version ====')
            if not await version_match(obj_transport, args.version, args.verbose):
                raise RuntimeError('Error in protocol version matching')
            print('==== Verified protocol version successfully ====')

        print('\n==== Starting Session ====')
        if not await establish_session(obj_transport, obj_security):
            print('Failed to establish session. Ensure that security scheme and proof of possession are correct')
            raise RuntimeError('Error in establishing session')
        print('==== Session Established ====')

        if args.reset:
            print('==== Reseting WiFi====')
            await reset_wifi(obj_transport, obj_security)
            sys.exit()

        if args.reprov:
            print('==== Reprovisioning WiFi====')
            await reprov_wifi(obj_transport, obj_security)
            sys.exit()

        print('\n==== Obtaining AWS IoT Provisioning Data ====')
        claim_data = create_provisioning_claim(args.template_name, args.aws_region)
        if claim_data is None:
            raise RuntimeError('Failed to create provisioning claim.')

        # Add the AWS endpoint to the JSON object
        claim_data["aws_endpoint"] = args.aws_endpoint

        # Download the Amazon Root CA certificate
        root_ca = download_root_ca()
        if root_ca:
            claim_data["root_ca"] = root_ca

        print('\n==== Sending AWS data to Target ====')
        if not await custom_data(obj_transport, obj_security, claim_data):
            raise RuntimeError('Error in custom data')
        print('==== Custom data sent successfully ====')

        if args.ssid == '':
            if not await has_capability(obj_transport, 'wifi_scan'):
                raise RuntimeError('Wi-Fi Scan List is not supported by provisioning service')

            while True:
                print('\n==== Scanning Wi-Fi APs ====')
                start_time = time.time()
                APs = await scan_wifi_APs(args.mode.lower(), obj_transport, obj_security)
                end_time = time.time()
                print('\n++++ Scan finished in ' + str(end_time - start_time) + ' sec')
                if APs is None:
                    raise RuntimeError('Error in scanning Wi-Fi APs')

                if len(APs) == 0:
                    print('No APs found!')
                    sys.exit()

                print('==== Wi-Fi Scan results ====')
                print('{0: >4} {1: <33} {2: <12} {3: >4} {4: <4} {5: <16}'.format(
                    'S.N.', 'SSID', 'BSSID', 'CHN', 'RSSI', 'AUTH'))
                for i in range(len(APs)):
                    print('[{0: >2}] {1: <33} {2: <12} {3: >4} {4: <4} {5: <16}'.format(
                        i + 1, APs[i]['ssid'], APs[i]['bssid'], APs[i]['channel'], APs[i]['rssi'], APs[i]['auth']))

                while True:
                    try:
                        select = int(input('Select AP by number (0 to rescan) : '))
                        if select < 0 or select > len(APs):
                            raise ValueError
                        break
                    except ValueError:
                        print('Invalid input! Retry')

                if select != 0:
                    break

            args.ssid = APs[select - 1]['ssid']

        if args.passphrase is None:
            prompt_str = 'Enter passphrase for {0} : '.format(args.ssid)
            args.passphrase = getpass(prompt_str)

        print('\n==== Sending Wi-Fi Credentials to Target ====')
        if not await send_wifi_config(obj_transport, obj_security, args.ssid, args.passphrase):
            raise RuntimeError('Error in send Wi-Fi config')
        print('==== Wi-Fi Credentials sent successfully ====')

        print('\n==== Applying Wi-Fi Config to Target ====')
        if not await apply_wifi_config(obj_transport, obj_security):
            raise RuntimeError('Error in apply Wi-Fi config')
        print('==== Apply config sent successfully ====')

        await wait_wifi_connected(obj_transport, obj_security)
    finally:
        await obj_transport.disconnect()

if __name__ == '__main__':
    asyncio.run(main())
