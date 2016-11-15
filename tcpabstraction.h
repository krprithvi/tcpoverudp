ssize_t SEND(int sockfd, const void *buf, size_t len, int flags);
int BIND(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int CONNECT(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int SOCKET(int domain, int type, int protocol);
ssize_t RECV(int sockfd, void *buf, size_t len, int flags);
int CONNECT(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int ACCEPT(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
int LISTEN(int sockfd, int backlog);
