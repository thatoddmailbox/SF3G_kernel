
/* ********************************************************************************************************************* */
/* оƬ���� */

/* �Ĵ����������� */

#define	RAM_ENDP0_SIZE		0x08		/* �˵�0�Ļ��������� */
#define	RAM_ENDP0_TRAN		0x20		/* �˵�0���ͻ���������ʼ��ַ */
#define	RAM_ENDP0_RECV		0x28		/* �˵�0���ջ���������ʼ��ַ */
#define	RAM_ENDP1_SIZE		0x08		/* �˵�1�Ļ��������� */
#define	RAM_ENDP1_TRAN		0x30		/* �˵�1���ͻ���������ʼ��ַ */
#define	RAM_ENDP1_RECV		0x38		/* �˵�1���ջ���������ʼ��ַ */
#define	RAM_ENDP2_SIZE		0x40		/* �˵�2�Ļ��������� */
#define	RAM_ENDP2_TRAN		0x40		/* �˵�2���ͻ���������ʼ��ַ */
#define	RAM_ENDP2_RECV		0xC0		/* �˵�2���ջ���������ʼ��ַ */
#define	RAM_ENDP2_EXCH		0x80		/* �˵�2���û���������ʼ��ַ */
#define	RAM_HOST_TRAN		0x40		/* �������ͻ���������ʼ��ַ */
#define	RAM_HOST_RECV		0xC0		/* �������ջ���������ʼ��ַ */
#define	RAM_HOST_EXCH		0x80		/* �������û���������ʼ��ַ */

#define	REG_HUB_SETUP		0x02		/* ��USB������ʽ: ROOT-HUB���� */
#define	REG_HUB_CTRL		0x03		/* ��USB������ʽ: ROOT-HUB���� */
#define	REG_SYS_INFO		0x04		/* ϵͳ��Ϣ, ֻ�� */
#define	REG_SYS_CTRL		0x05		/* ϵͳ����, ���������λӰ�� */
#define	REG_USB_SETUP		0x06		/* USB���� */
#define	REG_INTER_EN		0x07		/* �ж�ʹ�� */
#define	REG_USB_ADDR		0x08		/* USB�豸��ַ */
#define	REG_INTER_FLAG		0x09		/* �жϱ�־, ֻ��, λд1��0 */
#define	REG_USB_STATUS		0x0A		/* USB״̬, ֻ�� */
#define	REG_USB_LENGTH		0x0B		/* USB����, ��Ϊ��ǰUSB���ճ���, �豸��ʽ��дΪUSB�˵�2, ������ʽ��дΪUSB�������ͳ��� */
#define	REG_USB_ENDP0		0x0C		/* ��USB�豸��ʽ: USB�˵�0���� */
#define	REG_USB_ENDP1		0x0D		/* ��USB�豸��ʽ: USB�˵�1���� */
#define	REG_USB_H_PID		0x0D		/* ��USB������ʽ: USB�������� */
#define	REG_USB_ENDP2		0x0E		/* ��USB�豸��ʽ: USB�˵�2���� */
#define	REG_USB_H_CTRL		0x0E		/* ��USB������ʽ: USB�������� */

/* �Ĵ�����λ�����ú궨�� */

#define	BIT_HUB0_EN			0x01		/* HUB0�˿ڵ�USB����ʹ��: 0-��ֹ, 1-���� */
#define	BIT_HUB0_RESET		0x02		/* HUB0�˿ڵ�USB���߸�λ����: 0-����λ, 1-��λ */
#define	BIT_HUB0_POLAR		0x04		/* HUB0�˿ڵ��źż��Կ���: 0-������/ȫ��, 1-������/���� */
#define	BIT_HUB0_ATTACH		0x08		/* HUB0�˿ڵ�USB�豸����״̬(ֻ��): 0-��δ����/�Ͽ�/�γ�, 1-�Ѿ�����/���� */
#define	BIT_HUB1_DX_IN		0x10		/* HUB1ȫ��������ʱUD+����/���ٸ�����ʱUD-���ŵĲ���״̬: 0-�͵�ƽ,�ٶ�ʧ��, 1-�ߵ�ƽ,�ٶ�ƥ�� */
#define	BIT_HUB2_DX_IN		0x20		/* HUB2ȫ��������ʱUD+����/���ٸ�����ʱUD-���ŵĲ���״̬: 0-�͵�ƽ,�ٶ�ʧ��, 1-�ߵ�ƽ,�ٶ�ƥ�� */
#define	BIT_HUB_PRE_PID		0x40		/* ����ǰ�ð�PRE PID�������: 0-��ֹ, 1-����(�ⲿ�豸��USB-HUB) */
#define	BIT_HUB_DISABLE		0x80		/* ��ֹROOT-HUB������������: 0-����(��USB������ʽ), 1-��ֹ(Ĭ��) */

#define	BIT_HUB1_EN			0x01		/* HUB1�˿ڵ�USB����ʹ��: 0-��ֹ, 1-���� */
#define	BIT_HUB1_RESET		0x02		/* HUB1�˿ڵ�USB���߸�λ����: 0-����λ, 1-��λ */
#define	BIT_HUB1_POLAR		0x04		/* HUB1�˿ڵ��źż��Կ���: 0-������/ȫ��, 1-������/���� */
#define	BIT_HUB1_ATTACH		0x08		/* HUB1�˿ڵ�USB�豸����״̬(ֻ��): 0-��δ����/�Ͽ�/�γ�, 1-�Ѿ�����/���� */
#define	BIT_HUB2_EN			0x10		/* HUB2�˿ڵ�USB����ʹ��: 0-��ֹ, 1-���� */
#define	BIT_HUB2_RESET		0x20		/* HUB2�˿ڵ�USB���߸�λ����: 0-����λ, 1-��λ */
#define	BIT_HUB2_POLAR		0x40		/* HUB2�˿ڵ��źż��Կ���: 0-������/ȫ��, 1-������/���� */
#define	BIT_HUB2_ATTACH		0x80		/* HUB2�˿ڵ�USB�豸����״̬(ֻ��): 0-��δ����/�Ͽ�/�γ�, 1-�Ѿ�����/���� */

#define	BIT_INFO_HW_ID		0x03		/* Ӳ��ʶ��λ: ���ǳ���01, ����˵����������Ӳ���������� */
#define	BIT_INFO_USB_DM		0x04		/* USB����HUB0��UD-���ŵ��߼���ƽ״̬: 0-�͵�ƽ, 1-�ߵ�ƽ */
#define	BIT_INFO_USB_DP		0x08		/* USB����HUB0��UD+���ŵ��߼���ƽ״̬: 0-�͵�ƽ, 1-�ߵ�ƽ */
#define	BIT_INFO_CLK_8KHZ	0x10		/* Ӳ��8KHzʱ��λ */
#define	BIT_INFO_SOF_PRES	0x20		/* Ӳ��1mS��ʱ����״̬,����USB����, 1˵����Ҫ����SOF */
#define	BIT_INFO_WAKE_UP	0x40		/* оƬ����״̬: 0-����˯�߻��ѹ�����, 1-�ѻ��� */
#define	BIT_INFO_POWER_RST	0x80		/* Ӳ���ϵ縴λ���״̬: 0-���ڸ�λ, 1-��λ��� */

#define	BIT_CTRL_OSCIL_OFF	0x01		/* ʱ����������: 0-������, 1-ֹͣ�� */
#define	BIT_CTRL_CLK_12MHZ	0x02		/* ����ʱ��Ƶ��ѡ��: 0-24MHz, 1-12MHz */
#define	BIT_CTRL_USB_POWER	0x04		/* V3���ŵ�USB��Դ����������: 0-����, 1-���� */
#define	BIT_CTRL_RESET_NOW	0x08		/* оƬ�����λ����: 0-����λ, 1-��λ */
#define	BIT_CTRL_WATCH_DOG	0x10		/* RST���ź�RST#���ŵĿ��Ź���λʹ��: 0-����, 1-���� */
#define	BIT_CTRL_INT_PULSE	0x20		/* INT#���ŵ��ж������ʽ: 0-�͵�ƽ�ж�, 1-�͵�ƽ�����ж� */
#define	BIT_CTRL_OE_POLAR	0x40		/* UEN���ŵ�USB���ʹ�ܼ���: 0-�ߵ�ƽʹ��, 1-�͵�ƽʹ�� */

#define	BIT_SETP_TRANS_EN	0x01		/* ��USB�豸��ʽ: USB�豸����ʹ��: 0-��ֹ, 1-���� */
#define	BIT_SETP_PULLUP_EN	0x02		/* ��USB�豸��ʽ: USB�����������: 0-������������, 1-������������ */
#define	BIT_SETP_BUS_CTRL	0x03		/* ��USB������ʽ: USB����״̬����: 00-����/����, 01-D+��D-��(���߸�λ), 10-����, 11-D+��D-��(���߻ָ�) */
#define	M_SET_USB_BUS_FREE( old )		( (old) & ~ BIT_SETP_BUS_CTRL | 0x00 )		/* ��USB������ʽ: USB���߿��� */
#define	M_SET_USB_BUS_RESET( old )		( (old) & ~ BIT_SETP_BUS_CTRL | 0x01 )		/* ��USB������ʽ: USB����״̬����/D+��D-��(���߸�λ) */
#define	M_SET_USB_BUS_RESUME( old )		( (old) & ~ BIT_SETP_BUS_CTRL | 0x03 )		/* ��USB������ʽ: USB����״̬����/D+��D-��(���߻ָ�) */
#define	BIT_SETP_RAM_MODE	0x0C		/* ���û�����Ӧ�÷�ʽ: 00-���ñ��û�����, 01-���ӽ��ջ���������������128�ֽ�, 10-���ڷ��͵ڶ�������, 11-���ڽ��յڶ������� */
#define	M_SET_RAM_MODE_OFF( old )		( (old) & ~ BIT_SETP_RAM_MODE | 0x00 )		/* ���û�������ʽ/���ñ��û����� */
#define	M_SET_RAM_MODE_128( old )		( (old) & ~ BIT_SETP_RAM_MODE | 0x04 )		/* ���û�������ʽ/���ӽ��ջ���������������128�ֽ� */
#define	M_SET_RAM_MODE_2TX( old )		( (old) & ~ BIT_SETP_RAM_MODE | 0x08 )		/* ���û�������ʽ/���ڷ��͵ĵڶ�������,֧���������� */
#define	M_SET_RAM_MODE_2RX( old )		( (old) & ~ BIT_SETP_RAM_MODE | 0x0C )		/* ���û�������ʽ/���ڽ��յĵڶ�������,֧���������� */
#define	BIT_SETP_LOW_SPEED	0x20		/* USB���ߴ����ٶ�: 0-12Mbps, 1-1.5Mbps */
#define	BIT_SETP_USB_SPEED	0x30		/* USB��������: 00-ȫ��ģʽ/������12Mbps, 11-����ģʽ/������1.5Mbps */
#define	BIT_SETP_LED_ACT	0x40		/* ��USB�豸��ʽ: ACT#���ŵ͵�ƽ�ļ����¼�: 0-�շ��������, 1-USB����� */
#define	BIT_SETP_AUTO_SOF	0x40		/* ��USB������ʽ: �Զ�����SOFʹ��: 0-��ֹ, 1-���� */
#define	BIT_SETP_HOST_MODE	0x80		/* USB���ӷ�ʽѡ��: 0-�豸��ʽ, 1-������ʽ */

#define	BIT_IE_TRANSFER		0x01		/* USB��������ж�ʹ��, 1��Ч */
#define	BIT_IE_BUS_RESET	0x02		/* ��USB�豸��ʽ: USB���߸�λ�ж�ʹ��, 1��Ч */
#define	BIT_IE_DEV_DETECT	0x02		/* ��USB������ʽ: USB�豸����ж�ʹ��, 1��Ч */
#define	BIT_IE_USB_SUSPEND	0x04		/* USB���߹����ж�ʹ��, 1��Ч */
#define	BIT_IE_USB_RESUME	0x08		/* USB���߻ָ�/�����ж�ʹ��, 1��Ч, 0-ʹ��оƬ��������ж�, 1-ʹ��USB���߻ָ��ж� */
#define	BIT_IE_CLK_OUT_DIV	0xF0		/* �ɱ��ʱ�ӵķ�Ƶ����: ���Ƶ��Ϊ48MHz/(��ֵ+1), ����: 0000-48MHz, 0001-24MHz, 0010-16MHz, 1111-3MHz */
#define	M_SET_CLK_DIV( old, div )		( (old) & ~ BIT_IE_CLK_OUT_DIV | (div) << 4 )	/* ����ʱ�������Ƶ���� */

#define	BIT_ADDR_USB_DEV	0x7F		/* ���豸��ʽ��Ϊ������ΪUSB�豸�ĵ�ַ, ��������ʽ��Ϊ��ǰ��������USB�豸��ַ */

#define	BIT_IF_INTER_FLAG	0x0F		/* ���е�USB�жϱ�־ */
#define	BIT_IF_TRANSFER		0x01		/* USB��������жϱ�־, 1��Ч, ���λд1���־, ��λ��ÿ��USB������ɺ��Զ���1 */
#define	BIT_IF_BUS_RESET	0x02		/* ��USB�豸��ʽ: USB���߸�λ�жϱ�־, 1��Ч, ���λд1���־, ��λ�ڼ�⵽USB���߸�λʱ�Զ���1 */
#define	BIT_IF_DEV_DETECT	0x02		/* ��USB������ʽ: USB�豸��μ���жϱ�־, 1��Ч, ���λд1���־, ��λ�ڼ�⵽USB�豸��κ��Զ���1 */
#define	BIT_IF_USB_SUSPEND	0x04		/* USB���߹����жϱ�־, 1��Ч, ���λд1���־, ��λ�ڼ�⵽USB���߹���ʱ�Զ���1 */
#define	BIT_IF_WAKE_UP		0x08		/* оƬ��������жϱ�־, 1��Ч, ���λд1���־, ��λ��оƬ������ɺ��Զ���1 */
#define	BIT_IF_USB_RESUME	0x08		/* USB���߻ָ�/�����жϱ�־, 1��Ч, ���λд1���־, ��λ�ڼ�⵽USB���߻ָ�ʱ�Զ���1 */
#define	BIT_IF_USB_PAUSE	0x10		/* USB������ͣ��־, 1��Ч, ���λд1���־, ��λ��ÿ��USB������ɺ��Զ���1 */
#define	BIT_IF_DEV_ATTACH	0x20		/* USB�豸����״̬: 0-��δ����/�Ͽ�/�γ�, 1-������һ��USB�豸�Ѿ�����/���� */
#define	BIT_IF_USB_OE		0x40		/* UEN���ŵ�USB���ʹ��״̬: 0-UEN����Ϊ�͵�ƽ, 1-UEN����Ϊ�ߵ�ƽ */
#define	BIT_IF_USB_DX_IN	0x80		/* HUB0ȫ��������ʱUD+����/���ٸ�����ʱUD-���ŵĲ���״̬: 0-�͵�ƽ,�ٶ�ʧ��, 1-�ߵ�ƽ,�ٶ�ƥ�� */
#define	BIT_HUB0_DX_IN		0x80		/* HUB0ȫ��������ʱUD+����/���ٸ�����ʱUD-���ŵĲ���״̬: 0-�͵�ƽ,�ٶ�ʧ��, 1-�ߵ�ƽ,�ٶ�ƥ�� */

#define	BIT_STAT_THIS_ENDP	0x03		/* ��USB�豸��ʽ: USB�����Ŀ�Ķ˵��: 00-�˵�0, 01-�˵�1, 10-�˵�2, 11-���� */
#define	BIT_STAT_THIS_PID	0x0C		/* ��USB�豸��ʽ: USB���������/����PID: 00-OUT����, 01-����, 10-IN����, 11-SETUP���� */
#define	BIT_STAT_PID_ENDP	0x0F		/* ��USB�豸��ʽ: USB���������Ͷ˵��,�ο������USB_INT_EP*���� */
#define	BIT_STAT_DEV_RESP	0x0F		/* ��USB������ʽ: USB�豸��Ӧ��PID: XX00=�����ʱ,����ֵ-ͬPID����,�ο������USB_INT_RET_*���� */
#define	M_IS_HOST_TIMEOUT( status )		( ( (status) & 0x03 ) == 0 )		/* ���USB����״̬�Ƿ�ΪӦ��ʱ/���� */
#define	M_IS_HOST_IN_DATA( status )		( ( (status) & BIT_STAT_DEV_RESP & ~ ( DEF_USB_PID_DATA0 ^ DEF_USB_PID_DATA1 ) ) == ( DEF_USB_PID_DATA0 & DEF_USB_PID_DATA1 ) )	/* ����Ƿ񷵻�DATA0����DATA1 */
#define	BIT_STAT_TOG_MATCH	0x10		/* ָʾ��ǰ�Ĵ����Ƿ�ɹ�/��ǰ���յ����ݰ��Ƿ�ͬ��: 0-��ͬ��, 1-ͬ�� */
#define	BIT_STAT_BUS_RESET	0x20		/* ��ǰUSB���߸�λ״̬: 0-û�и�λ, 1-���ڸ�λ */
#define	BIT_STAT_SUSPEND	0x40		/* ��ǰUSB���߹���״̬: 0-�����л, 1-���߹��� */
#define	BIT_STAT_SIE_FREE	0x80		/* ��ǰUSB�ӿ�����SIE��״̬: 0=æ/���ڴ���, 1=����/�ȴ� */

#define	BIT_EP0_TRAN_RESP	0x0F		/* ��USB�豸��ʽ: �˵�0������Ӧ: 0000~1000-Ӧ�����ݳ���0~8, 1110-Ӧ��NAK, 1111-Ӧ��STALL,����ֵ-���� */
#define	M_SET_EP0_TRAN_ACK( old, len )	( (old) & ~ BIT_EP0_TRAN_RESP | (len) & 0x0F )	/* ��USB�豸��ʽ: �˵�0������Ӧ/Ӧ��ACK */
#define	M_SET_EP0_TRAN_NAK( old )		( (old) & ~ BIT_EP0_TRAN_RESP | 0x0E )		/* ��USB�豸��ʽ: �˵�0������Ӧ/Ӧ��NAK */
#define	M_SET_EP0_TRAN_STA( old )		( (old) & ~ BIT_EP0_TRAN_RESP | 0x0F )		/* ��USB�豸��ʽ: �˵�0������Ӧ/Ӧ��STALL */
#define	BIT_EP0_RECV_RESP	0x30		/* ��USB�豸��ʽ: �˵�0������Ӧ: 00-Ӧ��ACK, 01-����, 10-Ӧ��NAK, 11-Ӧ��STALL */
#define	M_SET_EP0_RECV_ACK( old )		( (old) & ~ BIT_EP0_RECV_RESP | 0x00 )		/* ��USB�豸��ʽ: �˵�0������Ӧ/Ӧ��ACK */
#define	M_SET_EP0_RECV_NAK( old )		( (old) & ~ BIT_EP0_RECV_RESP | 0x20 )		/* ��USB�豸��ʽ: �˵�0������Ӧ/Ӧ��NAK */
#define	M_SET_EP0_RECV_STA( old )		( (old) & ~ BIT_EP0_RECV_RESP | 0x30 )		/* ��USB�豸��ʽ: �˵�0������Ӧ/Ӧ��STALL */
#define	BIT_EP0_TRAN_TOG	0x40		/* ��USB�豸��ʽ: �˵�0����ͬ����־: 0-DATA0, 1-DATA1 */
#define	BIT_EP0_RECV_TOG	0x80		/* ��USB�豸��ʽ: �˵�0����ͬ����־: 0-DATA0, 1-DATA1 */

#define	BIT_EP1_TRAN_RESP	0x0F		/* ��USB�豸��ʽ: �˵�1������Ӧ: 0000~1000-Ӧ�����ݳ���0~8, 1110-Ӧ��NAK, 1111-Ӧ��STALL,����ֵ-���� */
#define	M_SET_EP1_TRAN_ACK( old, len )	( (old) & ~ BIT_EP1_TRAN_RESP | (len) & 0x0F )	/* ��USB�豸��ʽ: �˵�1������Ӧ/Ӧ��ACK */
#define	M_SET_EP1_TRAN_NAK( old )		( (old) & ~ BIT_EP1_TRAN_RESP | 0x0E )		/* ��USB�豸��ʽ: �˵�1������Ӧ/Ӧ��NAK */
#define	M_SET_EP1_TRAN_STA( old )		( (old) & ~ BIT_EP1_TRAN_RESP | 0x0F )		/* ��USB�豸��ʽ: �˵�1������Ӧ/Ӧ��STALL */
#define	BIT_EP1_RECV_RESP	0x30		/* ��USB�豸��ʽ: �˵�1������Ӧ: 00-Ӧ��ACK, 01-����, 10-Ӧ��NAK, 11-Ӧ��STALL */
#define	M_SET_EP1_RECV_ACK( old )		( (old) & ~ BIT_EP1_RECV_RESP | 0x00 )		/* ��USB�豸��ʽ: �˵�1������Ӧ/Ӧ��ACK */
#define	M_SET_EP1_RECV_NAK( old )		( (old) & ~ BIT_EP1_RECV_RESP | 0x20 )		/* ��USB�豸��ʽ: �˵�1������Ӧ/Ӧ��NAK */
#define	M_SET_EP1_RECV_STA( old )		( (old) & ~ BIT_EP1_RECV_RESP | 0x30 )		/* ��USB�豸��ʽ: �˵�1������Ӧ/Ӧ��STALL */
#define	BIT_EP1_TRAN_TOG	0x40		/* ��USB�豸��ʽ: �˵�1����ͬ����־: 0-DATA0, 1-DATA1 */
#define	BIT_EP1_RECV_TOG	0x80		/* ��USB�豸��ʽ: �˵�1����ͬ����־: 0-DATA0, 1-DATA1 */

#define	BIT_HOST_PID_ENDP	0x0F		/* ��USB������ʽ: Ŀ�Ķ˵��: 0000~1111-�˵��0~15 */
#define	BIT_HOST_PID_TOKEN	0xF0		/* ��USB������ʽ: ָ������/����PID: 1101-SETUP����, 0001-OUT����, 1001-IN����, 0101-SOF��,����ֵ-���� */
#define	M_MK_HOST_PID_ENDP( pid, endp )	( ((pid) << 4) | ((endp) & BIT_HOST_PID_ENDP) )		/* ������/����PID��Ŀ�Ķ˵������USB������������ */

#define	BIT_EP2_TRAN_RESP	0x03		/* ��USB�豸��ʽ: �˵�2������Ӧ: 00-Ӧ������, 01-ͬ��/��ʱ����, 10-Ӧ��NAK, 11-Ӧ��STALL */
#define	M_SET_EP2_TRAN_ACK( old )		( (old) & ~ BIT_EP2_TRAN_RESP | 0x00 )		/* ��USB�豸��ʽ: �˵�2������Ӧ/Ӧ��ACK */
#define	M_SET_EP2_TRAN_ISO( old )		( (old) & ~ BIT_EP2_TRAN_RESP | 0x01 )		/* ��USB�豸��ʽ: �˵�2������Ӧ/ͬ��/��ʱ����/����Ӧ�� */
#define	M_SET_EP2_TRAN_NAK( old )		( (old) & ~ BIT_EP2_TRAN_RESP | 0x02 )		/* ��USB�豸��ʽ: �˵�2������Ӧ/Ӧ��NAK */
#define	M_SET_EP2_TRAN_STA( old )		( (old) & ~ BIT_EP2_TRAN_RESP | 0x03 )		/* ��USB�豸��ʽ: �˵�2������Ӧ/Ӧ��STALL */
#define	BIT_EP2_RECV_RESP	0x30		/* ��USB�豸��ʽ: �˵�2������Ӧ: 00-Ӧ��ACK, 01-ͬ��/��ʱ����, 10-Ӧ��NAK, 11-Ӧ��STALL */
#define	M_SET_EP2_RECV_ACK( old )		( (old) & ~ BIT_EP2_RECV_RESP | 0x00 )		/* ��USB�豸��ʽ: �˵�2������Ӧ/Ӧ��ACK */
#define	M_SET_EP2_RECV_ISO( old )		( (old) & ~ BIT_EP2_RECV_RESP | 0x10 )		/* ��USB�豸��ʽ: �˵�2������Ӧ/ͬ��/��ʱ����/����Ӧ�� */
#define	M_SET_EP2_RECV_NAK( old )		( (old) & ~ BIT_EP2_RECV_RESP | 0x20 )		/* ��USB�豸��ʽ: �˵�2������Ӧ/Ӧ��NAK */
#define	M_SET_EP2_RECV_STA( old )		( (old) & ~ BIT_EP2_RECV_RESP | 0x30 )		/* ��USB�豸��ʽ: �˵�2������Ӧ/Ӧ��STALL */
#define	BIT_EP2_TRAN_TOG	0x40		/* ��USB�豸��ʽ: �˵�2����ͬ����־: 0-DATA0, 1-DATA1 */
#define	BIT_EP2_RECV_TOG	0x80		/* ��USB�豸��ʽ: �˵�2����ͬ����־: 0-DATA0, 1-DATA1 */

#define	BIT_HOST_TRAN_ISO	0x01		/* ��USB������ʽ: �������͵Ĵ�������: 0-����/����/�жϴ���, 1-ͬ��/��ʱ���� */
#define	BIT_HOST_START		0x08		/* ��USB������ʽ: ����������������: 0-��ͣ, 1-��������,��ɺ��Զ���0 */
#define	BIT_HOST_RECV_ISO	0x10		/* ��USB������ʽ: �������յĴ�������: 0-����/����/�жϴ���, 1-ͬ��/��ʱ���� */
#define	BIT_HOST_TRAN_TOG	0x40		/* ��USB������ʽ: ��������ͬ����־: 0-DATA0, 1-DATA1 */
#define	BIT_HOST_RECV_TOG	0x80		/* ��USB������ʽ: ��������ͬ����־: 0-DATA0, 1-DATA1 */

