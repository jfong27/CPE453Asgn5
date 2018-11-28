


typedef struct partition_table {
   uint8_t bootind;
   uint8_t start_head;
   uint8_t start_sec;
   uint8_t start_cyl;
   uint8_t type;
   uint8_t end_head;
   uint8_t end_sec;
   uint8_t end_cyl;
   uint32_t lFirst;
   uint32_t size;
} p_table;
   
