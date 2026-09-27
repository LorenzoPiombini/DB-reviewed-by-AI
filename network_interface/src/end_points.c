#include "end_points.h"
#include <string.h>


int map_end_point(char *end_point)
{ 
	size_t s,so_s; 
	if(strstr(end_point,NEW_S_ORDER)){
		s = strlen(end_point);
		so_s = strlen(NEW_S_ORDER);
		if(s == so_s)
			if(strncmp(end_point,NEW_S_ORDER,so_s) == 0) return NEW_SORD;

	}

	if(strstr(end_point,NEW_CUSTOMER)){
		so_s = strlen(NEW_CUSTOMER);
		if(strncmp(end_point,NEW_CUSTOMER,so_s) == 0) return NEW_CUST;
	}


	if(strstr(end_point,REPORTS)){
		so_s = strlen(REPORTS);
		if(strncmp(end_point,REPORTS,so_s) == 0) return RPT;
	}

	/*
	 * ITEM SECTION Note: anything that contains '/items' should be tested before 
	 * this if statement
	 * */
	if(strstr(end_point,NEW_ITEM)){
		s = strlen(end_point);
		so_s = strlen(NEW_ITEM);

		if(s == so_s){
			if(strncmp(end_point,NEW_ITEM,so_s) == 0)
				return N_ITEM;
		}
	}

	if(strstr(end_point,ITEMS)){
		s = strlen(end_point);
		so_s = strlen(ITEMS);
		if(s == so_s){
			if(strncmp(end_point,ITEMS,so_s) == 0)
				return ITEM_GET_ALL;
		}else if(s > so_s){
			if(strncmp(end_point,ITEMS,so_s) == 0)
				return ITEM_GET;
		}
	}

	if(strstr(end_point,SALES_ORDERS)){
		s = strlen(end_point);
		so_s = strlen(SALES_ORDERS);
		if(s == so_s){
			if(strncmp(end_point,SALES_ORDERS,so_s) == 0) return S_ORD;
		}else if(s > so_s){
			/*if contains sales order return */
			if(strncmp(end_point,SALES_ORDERS,so_s) == 0) return S_ORD_GET;
		}
	}

	if(strstr(end_point,SALES_NEW_ORDER_CUSTOMERS))
		return S_ORD_CUSTOMER_GET;

	if(strstr(end_point,CUSTOMERS)){
		s = strlen(end_point);
		so_s = strlen(CUSTOMERS);
		if(s == so_s){
			if(strncmp(end_point,CUSTOMERS,so_s) == 0) return CUSTOMER_GET_ALL;
		}else if(s > so_s){
			if(strncmp(end_point,CUSTOMERS,so_s) == 0) return CUSTOMER_GET;
		}
	}


	if(strstr(end_point,SALES_ORDERS)){
		s = strlen(end_point);
		so_s = strlen(SALES_ORDERS);
		if(s == so_s){
			if(strncmp(end_point,SALES_ORDERS,so_s) == 0) return S_ORD;
		}else if(s > so_s){
			/*if contains sales order return */
			if(strncmp(end_point,SALES_ORDERS,so_s) == 0) return S_ORD_GET;
		}
	}

	
	if(strstr(end_point,UPDATE_ORDERS)){
		s = strlen(end_point);
		so_s = strlen(UPDATE_ORDERS);
		if(s == so_s){
			if(strncmp(end_point,UPDATE_ORDERS,so_s) == 0) return UPDATE_SORD;
		}else if(s > so_s){
			if(strncmp(end_point,UPDATE_ORDERS,so_s) == 0) return UPDATE_SORD;
		}
	}

	return -1;
}
