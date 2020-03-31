#include "net.h"

//-----------------------------------------------
uint8_t ipaddr_dest[4];

uint16_t port_dest;
extern UART_HandleTypeDef huart6;

USART_prop_ptr usartprop;
char str[30];
char str1[100];
u8_t data[100];

struct tcp_pcb *client_pcb;

enum client_states

{

  ES_NOT_CONNECTED = 0,

  ES_CONNECTED,

  ES_RECEIVED,

  ES_CLOSING,

};

struct client_struct

{

  enum client_states state; /* connection status */

  struct tcp_pcb *pcb; /* pointer on the current tcp_pcb */

  struct pbuf *p_tx; /* pointer on pbuf to be transmitted */

};

struct client_struct *cs;

__IO uint32_t message_count=0;

static err_t tcp_client_connected(void *arg, struct tcp_pcb *tpcb, err_t err);
static void tcp_client_connection_close(struct tcp_pcb *tpcb, struct client_struct * es);
static err_t tcp_client_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);

static void tcp_client_send(struct tcp_pcb *tpcb, struct client_struct * es);

static err_t tcp_client_sent(void *arg, struct tcp_pcb *tpcb, u16_t len);

static err_t tcp_client_poll(void *arg, struct tcp_pcb *tpcb);

void tcp_client_connect(void)

{
	 ip_addr_t DestIPaddr;

  client_pcb = tcp_new();

  if (client_pcb != NULL)

  {

    IP4_ADDR( &DestIPaddr, ipaddr_dest[0], ipaddr_dest[1], ipaddr_dest[2], ipaddr_dest[3]);

    tcp_connect(client_pcb,&DestIPaddr,port_dest,tcp_client_connected);

    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_14, GPIO_PIN_SET);

  }

}
uint16_t port_extract(char* ip_str, uint8_t len)

{

  uint16_t port=0;

  int ch1=':';

  char *ss1;

  uint8_t offset = 0;

  ss1=strchr(ip_str,ch1);

  offset=ss1-ip_str+1;

  ip_str+=offset;

  port = atoi(ip_str);

  return port;

}

void ip_extract(char* ip_str, uint8_t len, uint8_t* ipextp)

{

  uint8_t offset = 0;

  uint8_t i;

  char ss2[5] = {0};

  char *ss1;

  int ch1 = '.';

  int ch2 = ':';

  for(i=0;i<3;i++)

  {

    ss1 = strchr(ip_str,ch1);

    offset = ss1-ip_str+1;

    strncpy(ss2,ip_str,offset);

    ss2[offset]=0;

    ipextp[i] = atoi(ss2);

    ip_str+=offset;

    len-=offset;

  }

  ss1=strchr(ip_str,ch2);

  if (ss1!=NULL)

  {

    offset=ss1-ip_str+1;

    strncpy(ss2,ip_str,offset);

    ss2[offset]=0;

    ipextp[3] = atoi(ss2);

    return;

  }

  strncpy(ss2,ip_str,len);

  ss2[len]=0;

  ipextp[3] = atoi(ss2);

}

static err_t tcp_client_connected(void *arg, struct tcp_pcb *tpcb, err_t err)

{
 struct client_struct *es = NULL;

  if (err == ERR_OK)

  {
 es = (struct client_struct *)mem_malloc(sizeof(struct client_struct));

  if (es != NULL)

  {
es->state = ES_CONNECTED;

  es->pcb = tpcb;

  es->p_tx = pbuf_alloc(PBUF_TRANSPORT, strlen((char*)data) , PBUF_POOL);
		if (es->p_tx)

{

  /* copy data to pbuf */

  pbuf_take(es->p_tx, (char*)data, strlen((char*)data));

  /* pass newly allocated es structure as argument to tpcb */

  tcp_arg(tpcb, es);

  /* initialize LwIP tcp_recv callback function */

  tcp_recv(tpcb, tcp_client_recv);

  /* initialize LwIP tcp_sent callback function */

  tcp_sent(tpcb, tcp_client_sent);

  /* initialize LwIP tcp_poll callback function */

  tcp_poll(tpcb, tcp_client_poll, 1);

  /* send data */

  tcp_client_send(tpcb,es);

  return ERR_OK;

}
  }

  else

  {

    tcp_client_connection_close(tpcb, es);

    return ERR_MEM;

  }
  }

  else

  {

    tcp_client_connection_close(tpcb, es);

  }
  return err;

}

static void tcp_client_connection_close(struct tcp_pcb *tpcb, struct client_struct * es)

{

  /* remove callbacks */

  tcp_recv(tpcb, NULL);

  tcp_sent(tpcb, NULL);

  tcp_poll(tpcb, NULL,0);

  if (es != NULL)

  {

    mem_free(es);

  }

  /* close tcp connection */

  tcp_close(tpcb);

}

static err_t tcp_client_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)

{

  err_t ret_err;
	
	struct client_struct *es;

  return ret_err;
	es = (struct client_struct *)arg;
	if (p == NULL)

{
es->state = ES_CLOSING;

  if(es->p_tx == NULL)

  {

    tcp_client_connection_close(tpcb, es);

  }

  ret_err = ERR_OK;
}

else if(err != ERR_OK)

{
if (p != NULL)

  {

    pbuf_free(p);

  }

  ret_err = err;
}

else if(es->state == ES_CONNECTED)

{
 message_count++;

  tcp_recved(tpcb, p->tot_len);

  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_15, GPIO_PIN_SET);

  es->p_tx = p;

  strncpy(str1,es->p_tx->payload,es->p_tx->len);

  str1[es->p_tx->len] = '\0';

  HAL_UART_Transmit(&huart6, (uint8_t*)str1,strlen(str1),0x1000);

  ret_err = ERR_OK;
}

else if (es->state == ES_RECEIVED)

{
HAL_GPIO_WritePin(GPIOD, GPIO_PIN_13, GPIO_PIN_SET);

  ret_err = ERR_OK;
}

else

{
/* Acknowledge data reception */

  tcp_recved(tpcb, p->tot_len);

  /* free pbuf and do nothing */

  pbuf_free(p);

  ret_err = ERR_OK;
}

}

//----------------------------------------------------------

static void tcp_client_send(struct tcp_pcb *tpcb, struct client_struct * es)

{
 struct pbuf *ptr;

  err_t wr_err = ERR_OK;

  while ((wr_err == ERR_OK) &&

  (es->p_tx != NULL) &&

  (es->p_tx->len <= tcp_sndbuf(tpcb)))

  {
		ptr = es->p_tx;

  wr_err = tcp_write(tpcb, ptr->payload, ptr->len, 1);
		if (wr_err == ERR_OK)

{
 es->p_tx = ptr->next;

  if(es->p_tx != NULL)

  {

    pbuf_ref(es->p_tx);

  }

  pbuf_free(ptr);
}

else if(wr_err == ERR_MEM)

{
	 es->p_tx = ptr;
}

else

{

/* other problem ?? */

}

  }
}

//----------------------------------------------------------

static err_t tcp_client_sent(void *arg, struct tcp_pcb *tpcb, u16_t len)

{
 struct client_struct *es;

  LWIP_UNUSED_ARG(len);

  es = (struct client_struct *)arg;

  if(es->p_tx != NULL)

  {

    tcp_client_send(tpcb, es);

  }
  return ERR_OK;

}

//----------------------------------------------------------

static err_t tcp_client_poll(void *arg, struct tcp_pcb *tpcb)

{

  err_t ret_err;

struct client_struct *es;

es = (struct client_struct*)arg;

HAL_GPIO_TogglePin(GPIOD, GPIO_PIN_14);
	if (es != NULL)

{

  if (es->p_tx != NULL)

  {

  }

  else

  {

    if(es->state == ES_CLOSING)

    {

      tcp_client_connection_close(tpcb, es);

    }

  }

  ret_err = ERR_OK;

}
else

{

  tcp_abort(tpcb);

  ret_err = ERR_ABRT;

}
  return ret_err;

}
void net_cmd(char* buf_str)

{
	uint8_t ip[4];

  uint16_t port;

  if(usartprop.is_tcp_connect==1)

  {
		ip_extract(buf_str,usartprop.usart_cnt-1,ipaddr_dest);

  port_dest=port_extract(buf_str,usartprop.usart_cnt-1);

  usartprop.usart_cnt=0;

  usartprop.is_tcp_connect=0;

  tcp_client_connect();

  sprintf(str1,"%d.%d.%d.%d:%u\r\n", ipaddr_dest[0],ipaddr_dest[1],ipaddr_dest[2],ipaddr_dest[3],port_dest);

  HAL_UART_Transmit(&huart6,(uint8_t*)str1,strlen(str1),0x1000);

  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_13, GPIO_PIN_SET);

  }

  if(usartprop.is_tcp_connect==2)

  {
		 ip_extract(buf_str,usartprop.usart_cnt-1,ip);

  port=port_extract(buf_str,usartprop.usart_cnt-1);

  usartprop.usart_cnt=0;

  usartprop.is_tcp_connect=0;


  if(!memcmp(ip,ipaddr_dest,4))

  {

    if(port==port_dest)

    {

      /* close tcp connection */

      tcp_recv(client_pcb, NULL);

      tcp_sent(client_pcb, NULL);

      tcp_poll(client_pcb, NULL,0);

      tcp_close(client_pcb);

      HAL_GPIO_WritePin(GPIOD, GPIO_PIN_15, GPIO_PIN_SET);

    }

  }

  }

}

void sendstring(char* buf_str)

{

  tcp_sent(client_pcb, tcp_client_sent);

  tcp_write(client_pcb, (void*)buf_str, strlen(buf_str), 1);

  tcp_output(client_pcb);

  usartprop.usart_cnt=0;

  usartprop.is_text=0;

}

void string_parse(char* buf_str)

{
HAL_UART_Transmit(&huart6, (uint8_t*)buf_str,strlen(buf_str),0x1000);
	if (strncmp(buf_str,"t:", 2) == 0)

  {
usartprop.usart_cnt-=1;

  usartprop.is_tcp_connect=1;

  net_cmd(buf_str+2);

  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_12, GPIO_PIN_SET);
  }

  else if (strncmp(buf_str,"c:", 2) == 0)

  {
		usartprop.usart_cnt-=1;

  usartprop.is_tcp_connect=2;

  net_cmd(buf_str+2);

  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_13, GPIO_PIN_SET);

  }

  else

  {
usartprop.is_text=1;

  sendstring(buf_str);

  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_12|GPIO_PIN_13, GPIO_PIN_RESET);
  }

}

void net_ini(void)

{

  usartprop.usart_buf[0]=0;

  usartprop.usart_cnt=0;

  usartprop.is_tcp_connect=0;

  usartprop.is_text=0;

}

void UART6_RxCpltCallback(void)

{
uint8_t b;

  b = str[0];


  if (usartprop.usart_cnt>25)

  {

    usartprop.usart_cnt=0;

    HAL_UART_Receive_IT(&huart6,(uint8_t*)str,1);

    return;

  }
usartprop.usart_buf[usartprop.usart_cnt] = b;
	if(b==0x0A)

{

  usartprop.usart_buf[usartprop.usart_cnt+1]=0;
	string_parse((char*)usartprop.usart_buf);
	usartprop.usart_cnt=0;

  HAL_UART_Receive_IT(&huart6,(uint8_t*)str,1);

  return;

}
  usartprop.usart_cnt++;

  HAL_UART_Receive_IT(&huart6,(uint8_t*)str,1);
}