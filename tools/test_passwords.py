import sys
import paramiko

candidates = [
    '1234', '12345', '123456', '12345678', '123456789',
    'password', 'raspberry', 'andr', 'andrd', 'admin',
    'qwerty', 'pi', 'root', '1111'
]
pubkey = "ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAID+DcPaCsdeQDVe1p4E+gKDFES1cD0vaBOtG7FKU8gc+ andrd@DESKTOP-PA9BC5N"

for pwd in candidates:
    client = paramiko.SSHClient()
    client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    try:
        client.connect('192.168.178.51', port=22, username='andr', password=pwd, timeout=4)
        print(f"MATCH FOUND: {pwd}")
        cmd = f"""mkdir -p ~/.ssh && chmod 700 ~/.ssh && echo "{pubkey.strip()}" >> ~/.ssh/authorized_keys && chmod 600 ~/.ssh/authorized_keys && sort -u -o ~/.ssh/authorized_keys ~/.ssh/authorized_keys"""
        client.exec_command(cmd)
        print("SSH key installed!")
        client.close()
        sys.exit(0)
    except paramiko.AuthenticationException:
        print(f"Failed: {pwd}")
    except Exception as e:
        print(f"Error testing {pwd}: {e}")
    finally:
        client.close()

print("No match in quick candidate list.")
sys.exit(1)
