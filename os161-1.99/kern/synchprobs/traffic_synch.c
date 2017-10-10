#include <types.h>
#include <lib.h>
#include <synchprobs.h>
#include <synch.h>
#include <opt-A1.h>

/* 
 * This simple default synchronization mechanism allows only vehicle at a time
 * into the intersection.   The intersectionSem is used as a a lock.
 * We use a semaphore rather than a lock so that this code will work even
 * before locks are implemented.
 */

/* 
 * Replace this default synchronization mechanism with your own (better) mechanism
 * needed for your solution.   Your mechanism may use any of the available synchronzation
 * primitives, e.g., semaphores, locks, condition variables.   You are also free to 
 * declare other global variables if your solution requires them.
 */

/*
 * replace this with declarations of any synchronization and other variables you need here
 */
//static struct semaphore *intersectionSem;

static struct cv *car_can_go;
static struct lock *intersectionLock;
struct car{

	Direction o;
	Direction d;
	struct car * next;
};

volatile struct car *car_list;



bool
which_turn (Direction o, Direction d)
{
        bool i = false;

        switch (o)
                {
                case north:
                        if (d == west) i = true; 
                        break;
                case south:
                        if (d == east) i = true;
                        break;
                case west:
                        if (d == south) i = true;
                        break;
                case east:
                        if (d == north) i = true;
                        break;

                }
        return i;

}

bool car_cango (Direction o, Direction d) {

	volatile struct car * temp = car_list;
	while (temp) {
		if (temp->o == o && temp->d == d) { temp = temp->next; continue; }
		if (temp->o == d && temp->o == o) { temp= temp->next; continue; }
		if (temp->d != d && (which_turn(o,d) || which_turn(temp->o, temp->d))) {
		temp = temp->next;
		continue;}
		return false;
		

	}
	return true;

} 


void
intersection_sync_init(void)
{
  /* replace this default implementation with your own implementation */


	intersectionLock = lock_create("intersectionLock");
        if (intersectionLock == NULL){
                panic("could not create intersection lock");
        }

	car_can_go = cv_create("car_can_go");
	if (car_can_go == NULL){
		panic("could not create W_strgt_cango cv");

	}
	car_list = NULL;

  return;
}

void
intersection_sync_cleanup(void)
{
  /* replace this default implementation with your own implementation */
	//KASSERT(intersectionSem != NULL);
	//sem_destroy(intersectionSem);
	
	KASSERT(intersectionLock != NULL);
	lock_destroy(intersectionLock);
	
        KASSERT(car_can_go != NULL);
        cv_destroy(car_can_go);

	while(car_list) {
		volatile struct car * temp = car_list;
		car_list = car_list->next;
		kfree((struct car*)temp);
	}
}



/*
 * The simulation driver will call this function each time a vehicle
 * tries to enter the intersection, before it enters.
 * This function should cause the calling simulation thread 
 * to block until it is OK for the vehicle to enter the intersection.
 *
 * parameters:
 *    * origin: the Direction from which the vehicle is arriving
 *    * destination: the Direction in which the vehicle is trying to go
 *
 * return value: none
 */

void
intersection_before_entry(Direction origin, Direction destination) 
{
  /* replace this default implementation with your own implementation */
  (void)origin;  /* avoid compiler complaint about unused parameter */
  (void)destination; /* avoid compiler complaint about unused parameter */

	KASSERT(intersectionLock != NULL);

	KASSERT(car_can_go != NULL);

	lock_acquire(intersectionLock);

	while (!car_cango(origin, destination)){
		cv_wait(car_can_go, intersectionLock);
	}
	volatile struct car * temp = kmalloc(sizeof(struct car));
	temp->o = origin;
	temp->d = destination;
	temp->next = (struct car *)car_list;
	car_list = temp;
	lock_release(intersectionLock);



}


/*
 * The simulation driver will call this function each time a vehicle
 * leaves the intersection.
 *
 * parameters:
 *    * origin: the Direction from which the vehicle arrived
 *    * destination: the Direction in which the vehicle is going
 *
 * return value: none
 */

void
intersection_after_exit(Direction origin, Direction destination) 
{
  /* replace this default implementation with your own implementation */
  (void)origin;  /* avoid compiler complaint about unused parameter */
  (void)destination; /* avoid compiler complaint about unused parameter */
  //KASSERT(intersectionSem != NULL);
  //V(intersectionSem);
	KASSERT(intersectionLock != NULL);

        KASSERT(car_can_go != NULL);
        
	lock_acquire(intersectionLock);
	volatile struct car * temp = car_list;
	volatile struct car *pre = car_list;
	if (temp->o == origin && temp->d == destination) {
		car_list = car_list->next;
		kfree((struct car *)temp);
		cv_broadcast(car_can_go, intersectionLock);
		lock_release(intersectionLock);
		return;
	}
	temp = temp->next;
	
	while(temp) {
		if (temp->o == origin && temp->d == destination) {
			pre->next = temp->next;
			kfree((struct car *)temp);
			cv_broadcast(car_can_go, intersectionLock);
			lock_release(intersectionLock);
			return;
		}
	else { temp = temp->next; pre = pre->next;}


	}

	lock_release(intersectionLock);	


}
