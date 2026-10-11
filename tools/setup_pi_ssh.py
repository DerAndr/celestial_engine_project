import sys
import paramiko

def try_auth(host, user, password, pubkey):
    client = paramiko.SSHClient()
    client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    try:
        print(f"Connecting to {host} as {user} with password...")
        client.connect(host, port=22, username=user, password=password, timeout=5)
        print("Authentication SUCCESSFUL!")
        
        # Inject authorized_keys
        cmd = f"""mkdir -p ~/.ssh && chmod 700 ~/.ssh && echo "{pubkey.strip()}" >> ~/.ssh/authorized_keys && chmod 600 ~/.ssh/authorized_keys && sort -u -o ~/.ssh/authorized_keys ~/.ssh/authorized_keys"""
        stdin, stdout, stderr = client.exec_command(cmd)
        stdout.channel.recv_exit_status()
        print("Public key successfully installed to ~/.ssh/authorized_keys!")
        
        # Check system info
        stdin, stdout, stderr = client.exec_command("uname -a; echo '=== LSBLK ==='; lsblk; echo '=== LSPCI ==='; lspci")
        print(stdout.read().decode())
        client.close()
        return True
    except paramiko.AuthenticationException:
        print("Authentication failed (incorrect password).")
        client.close()
        return False
    except Exception as e:
        print(f"Error: {e}")
        client.close()
        return False

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python setup_pi_ssh.py <password> <pubkey>")
        sys.exit(1)
    pwd = sys.argv[1]
    pk = sys.argv[2]
    success = try_auth("192.168.178.51", "andr", pwd, pk)
    sys.exit(0 if success else 1)
