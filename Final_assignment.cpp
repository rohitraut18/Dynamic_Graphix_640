#include <notcurses/notcurses.h>
#include<time.h>
#include<pthread.h>
#include <cerrno>
struct shared_resources {
  struct ncplane* floating_plane[3];
  struct notcurses* nc_environ;
  pthread_mutex_t lock;
  int current_planeID; //0, 1, or 2
};

void* shrinking_box(void* _sh_res)
{
  struct shared_resources* res_ptr=(struct shared_resources*)_sh_res;
  unsigned shrink_box_by=0;
  unsigned length_of_smaller_dimension;
  while(true)
  {
    pthread_mutex_lock(&res_ptr->lock);
    /*This is current plane*/
    struct ncplane* curr = res_ptr->floating_plane[res_ptr->current_planeID];

    ncplane_erase(res_ptr->floating_plane[res_ptr->current_planeID]);
    //erase current plane
    unsigned int rows, cols;
    ncplane_dim_yx(curr, &rows, &cols);

    if(rows<cols)
    {
      length_of_smaller_dimension=rows;
    }
    else if(rows>cols)
    {
      length_of_smaller_dimension=cols;
    }
    else
    {
      length_of_smaller_dimension = rows;
    } 
    shrink_box_by=shrink_box_by%(length_of_smaller_dimension/2)+1;
    //reduce box size by one: shrink_box_by=++shrink_box_by%(length_of_smaller_dimension/2)
    //draw box
    uint64_t channel_white{0};
    ncchannels_set_fg_rgb8(&channel_white, 0xff, 0xff, 0xff);
    ncplane_cursor_move_yx(curr, shrink_box_by-1, shrink_box_by-1);
    ncplane_rounded_box(curr, 0, channel_white, rows-shrink_box_by, cols-shrink_box_by, 0);
    //draw drag square
    //use ncplane_dim_yx() to get the plane's height & width
    ncplane_cursor_move_yx(curr, rows-1, cols-2);
    const wchar_t glyph{L'\u2591'};
    ncplane_putwc_stained(curr, glyph);
    ncplane_putwc_stained(curr, glyph);
    notcurses_render(res_ptr->nc_environ);
    pthread_mutex_unlock(&res_ptr->lock);

    //delay for 1/6 second
    struct timespec deadline;
    clock_gettime(CLOCK_MONOTONIC, &deadline);
    const uint64_t ONE_SECOND = 1000000000ull;
    deadline.tv_nsec += ONE_SECOND/6;
    // Correct the time to account for the second boundary
    if(deadline.tv_nsec >= ONE_SECOND) {
      deadline.tv_nsec -= ONE_SECOND;
      ++deadline.tv_sec;
    }
    // The while loop makes the sleep call robust against signals.
    // Overkill for this assignment.
    while(clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &deadline, NULL)<0){
      if(errno != EINTR) return NULL;
    }
  }
  return nullptr;
}

int main()
{
  notcurses_options ncopts{0};
  struct notcurses* nc_environ;
  nc_environ=notcurses_core_init(&ncopts, nullptr);
  notcurses_mice_enable(nc_environ, NCMICE_ALL_EVENTS);
  struct shared_resources sh_res = {
    .nc_environ = nc_environ,
    .lock = PTHREAD_MUTEX_INITIALIZER,
    .current_planeID = 0
  };

  struct ncplane* std_plane=notcurses_stdplane(nc_environ);
  nccell my_underlying_cell=NCCELL_CHAR_INITIALIZER(' ');
  nccell_set_bg_rgb8(&my_underlying_cell, 0x44, 0x11, 0x44);
  ncplane_set_base_cell(std_plane, &my_underlying_cell);
  // sh_res.nc_environ = nc_environ;
  ncplane_set_fg_rgb8(std_plane, 0xee, 0x00, 0x33); 
  ncplane_set_bg_rgb8(std_plane, 0x33, 0x00, 0xee);

  //plane1
  struct ncplane_options plane_opts1{0};
  plane_opts1.rows= 5+rand()%10; 
  plane_opts1.cols= 10+rand()%20;
  plane_opts1.y=5+rand()%20; 
  plane_opts1.x=3+rand()%20;
  struct ncplane* plane1=ncplane_create(std_plane, &plane_opts1);
  nccell_set_bg_rgb8(&my_underlying_cell, 0xff, 0x00, 0x00);
  unsigned int r1 =0xff;
  unsigned int g1 =0x00;
  unsigned int b1 =0x00;
  ncplane_set_base_cell(plane1, &my_underlying_cell);
  sh_res.floating_plane[0]=plane1;

  //plane 2
  struct ncplane_options plane_opts2{0};
  plane_opts2.rows= 5+rand()%10; 
  plane_opts2.cols= 10+rand()%20;
  plane_opts2.y=5+rand()%20; 
  plane_opts2.x=5+rand()%20;
  struct ncplane* plane2=ncplane_create(std_plane, &plane_opts2);
  nccell_set_bg_rgb8(&my_underlying_cell, 0x00, 0xff, 0x00);
  unsigned int r2 =0x00;
  unsigned int g2 =0xff;
  unsigned int b2 =0x00;
  ncplane_set_base_cell(plane2, &my_underlying_cell);
  sh_res.floating_plane[1]=plane2;

  //plane 3
  struct ncplane_options plane_opts3{0};
  plane_opts3.rows= 5+rand()%10; 
  plane_opts3.cols= 10+rand()%20;
  plane_opts3.y=2+rand()%20; 
  plane_opts3.x=7+rand()%20;
  struct ncplane* plane3=ncplane_create(std_plane, &plane_opts3);
  nccell_set_bg_rgb8(&my_underlying_cell, 0x00, 0x00, 0xff);
  unsigned int r3 =0x00;
  unsigned int g3 =0x00;
  unsigned int b3 =0xff;
  ncplane_set_base_cell(plane3, &my_underlying_cell);
  sh_res.floating_plane[2]=plane3;

  // notcurses_render(nc_environ); 
  struct ncinput input{0};
  uint32_t prev_id=0;
  uint64_t channel{0};
  uint64_t channel_white{0};
  struct ncplane* current_plane=plane1;

  ncchannels_set_fg_rgb8(&channel, 0xff, 0x34, 0xee);
  ncchannels_set_fg_rgb8(&channel_white, 0xff, 0xff, 0xff);
  ncplane_perimeter_rounded(current_plane, 0 ,0, channel);

  pthread_t tid;
  sh_res.current_planeID = 0;
  pthread_create(&tid, nullptr, shrinking_box, &sh_res);
  //notcurses_render(nc_environ);
  struct ncplane* target_plane;
  int y_local, x_local;//declare these two variables *before* the main loop
  int y_origin, x_origin;
  bool currently_resizing;
  do 
  {
    notcurses_get_blocking(nc_environ, &input);
    pthread_mutex_lock(&sh_res.lock);
    switch(input.id)
    {
      case NCKEY_BUTTON1:
        if(prev_id!=NCKEY_BUTTON1) { //button just pressed
                                     //1. Set a "currently_resizing" flag to false
          ncplane_erase(sh_res.floating_plane[sh_res.current_planeID]);
          currently_resizing = false;
          //2. Determine the "target_plane", the plane the mouse clicked on
          target_plane=notcurses_top(nc_environ);//start with top plane
          y_local=input.y, x_local=input.x;
          while(target_plane!=std_plane && ncplane_translate_abs(target_plane, &y_local, &x_local)==false)
          {
            y_local=input.y, x_local=input.x;
            target_plane=ncplane_below(target_plane);
          }
          //3. If the target_plane is NOT the std_plane, then:
          if(target_plane!=std_plane)
          {
            current_plane = target_plane;
            if(current_plane == plane1)
            {
              sh_res.current_planeID=0;
            }
            if(current_plane == plane2)
            {
              sh_res.current_planeID=1;
            }
            if(current_plane == plane3)
            {
              sh_res.current_planeID=2;
            }
          }
          unsigned int rows, cols;
          ncplane_dim_yx(current_plane, &rows, &cols);
          if(rows-y_local<=2 && cols-x_local<=2)
          {
            currently_resizing=true;
          }
          //   a. Make the current plane the same as the target plane
          //   b. If the mouse was clicked on the plane's resize box,
          //      then set currently_resizing=true.
        }
        if(prev_id==NCKEY_BUTTON1) { //button still pressed
                                     //1. If currently_resizing==false, and target_plane!=std_plane:
                                     //   call ncplane_move_xy() to move the plane
          if(currently_resizing==false && target_plane!=std_plane)
          {
            ncplane_move_yx(target_plane, input.y-y_local, input.x-x_local);
          }

          //2. If currently_resizing==true, and target_plane!=std_plane:
          //   call ncplane_resize_simple()
          ncplane_abs_yx(current_plane, &y_origin, &x_origin);
          if(currently_resizing==true && target_plane!=std_plane)
          {
            int y = input.y-y_origin+1;
            if(y<3) y=3;
            int x = input.x-x_origin+1;
            if(x<3) x=3;
            ncplane_resize_simple(target_plane,y, x);
          }
        }
        break;
      case 'h':
        ncplane_move_rel(current_plane, 0, -1);
        break;
      case 'j':
        ncplane_move_rel(current_plane, 1, 0);
        break;
      case 'k':
        ncplane_move_rel(current_plane, -1, 0);
        break;
      case 'l':
        ncplane_move_rel(current_plane, 0, 1);
        break;
      case ' ':
        if(current_plane==plane1)
        {
          // ncchannels_set_fg_rgb8(&channel, r1, g1, b1);
          // ncplane_perimeter_rounded(current_plane, 0 ,channel, 0);
          current_plane = plane2;
          ncplane_erase(sh_res.floating_plane[sh_res.current_planeID]);
          sh_res.current_planeID=1;
          // ncplane_perimeter_rounded(current_plane, 0 ,channel_white, 0);
        }
        else if(current_plane == plane2)
        {
          // ncchannels_set_fg_rgb8(&channel, r2, g2, b2);
          // ncplane_perimeter_rounded(current_plane, 0 ,channel, 0);
          current_plane = plane3;
          ncplane_erase(sh_res.floating_plane[sh_res.current_planeID]);
          sh_res.current_planeID=2;
          // ncplane_perimeter_rounded(current_plane, 0 ,channel_white, 0);
        }
        else if(current_plane == plane3)
        {
          // ncchannels_set_fg_rgb8(&channel, r3, g3, b3);
          // ncplane_perimeter_rounded(current_plane, 0 ,channel, 0);
          current_plane = plane1;
          ncplane_erase(sh_res.floating_plane[sh_res.current_planeID]);
          sh_res.current_planeID=0;
          // ncplane_perimeter_rounded(current_plane, 0 ,channel_white, 0);
        }
        break;

      case 'c':
        if(current_plane==plane1)
        {
          nccell_set_bg_rgb8(&my_underlying_cell, rand()%256, rand()%256, rand()%256);
          ncplane_set_base_cell(current_plane, &my_underlying_cell);
        }
        else if(current_plane == plane2)
        {
          nccell_set_bg_rgb8(&my_underlying_cell, rand()%256, rand()%256, rand()%256);
          ncplane_set_base_cell(current_plane, &my_underlying_cell);
        }
        else if(current_plane == plane3)
        {
          nccell_set_bg_rgb8(&my_underlying_cell, rand()%256, rand()%256, rand()%256);
          ncplane_set_base_cell(current_plane, &my_underlying_cell);
        }
        break;

      case 'z':
        ncplane_move_top(current_plane);
        break;
    }
    notcurses_render(nc_environ);
    prev_id=input.id;
    pthread_mutex_unlock(&sh_res.lock);
  }
  while(input.id!='q');
  notcurses_stop(nc_environ);
}
