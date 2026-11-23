import socket
import sys

def run_server(port):
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    try:
        server.bind(('127.0.0.1', port))
        server.listen(64)  # backlog 의 매개변수는 버전 3.5 부터 선택적
        
        print(f"server started (127.0.0.1:{port})")
        
        while True:
            conn, addr = server.accept() # 반환 값은 (conn, address) 쌍 임
            print(f"connected from {addr}")
            
            try :
                while True:
                    data = conn.recv(2048)
                    if not data:
                        print(f"client disconnected ({addr})")
                        break
                    
                    resp = f"port {port} echo :".encode() + data
                    conn.sendall(resp)
                
            except Exception as e:
                print(f"client error: {e}")
            
            finally:
                conn.close()
            
    except KeyboardInterrupt:
        print("\ninterrupted by user")
    
    except Exception as e:
        print(f"error: {e}")
    
    finally:
        server.close()
        print("socket closed")
        
        
import sys

if __name__ == '__main__':
    if len(sys.argv) > 1 and sys.argv[1].isdigit():
        port = int(sys.argv[1])
        run_server(port)
    else:
        print("argument err")