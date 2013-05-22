// (c) by Stefan Roettger, licensed under GPL 2+

#define SOBEL
#define MULTILEVEL

#undef FBOMM
#define FBO16

#include "volume.h"

BOOLINT volume::HASFBO=FALSE;
BOOLINT volume::USEFBO=FALSE;
int volume::fboWidth=0,volume::fboHeight=0;
GLuint volume::textureId=0;
GLuint volume::rboId=0;
GLuint volume::fboId=0;

volume::volume(tfunc2D *tf,char *base)
   {
   TILEMAX=TILEINC;
   TILE=new tileptr[TILEMAX];
   TILECNT=0;

   TFUNC=tf;

   if (base==NULL) strncpy(BASE,"volren",MAXSTR);
   else snprintf(BASE,MAXSTR,"%s/volren",base);
   }

volume::~volume()
   {
   int i;

   for (i=0; i<TILECNT; i++) delete TILE[i];
   delete TILE;

   destroy();
   }

// create fbo
void volume::setup(int width,int height)
   {
   char *GL_EXTs;

   if (!HASFBO && fboWidth==0 && fboHeight==0)
      {
      fboWidth=width;
      fboHeight=height;

      return;
      }

   if ((GL_EXTs=(char *)glGetString(GL_EXTENSIONS))==NULL) ERRORMSG();

   if (strstr(GL_EXTs,"EXT_framebuffer_object")!=NULL)
      if (width>0 && height>0)
         if (!HASFBO || width!=fboWidth || height!=fboHeight)
            {
#ifdef GL_EXT_framebuffer_object

            destroy();

            HASFBO=TRUE;

            // save actual size
            fboWidth=width;
            fboHeight=height;

            // create a texture object
            glGenTextures(1, &textureId);
            glBindTexture(GL_TEXTURE_2D, textureId);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
#ifdef FBOMM
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
#else
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
#endif
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
#ifdef FBOMM
            glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE); // automatic mipmap
#else
            glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_FALSE); // automatic mipmap off
#endif
#ifdef FBO16
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F_ARB, width, height, 0, GL_RGBA, GL_HALF_FLOAT_ARB, 0);
#else
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
#endif
            glBindTexture(GL_TEXTURE_2D, 0);

            // create a renderbuffer object to store depth info
            glGenRenderbuffersEXT(1, &rboId);
            glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, rboId);
            glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_DEPTH_COMPONENT, width, height);
            glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, 0);

            // create a framebuffer object
            glGenFramebuffersEXT(1, &fboId);
            glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fboId);

            // attach the texture to fbo color attachment point
            glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, textureId, 0);

            // attach the renderbuffer to depth attachment point
            glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, rboId);

            // get fbo status
            GLenum status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);

            // switch back to window-system-provided framebuffer
            glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);

            // check fbo status
            if (status != GL_FRAMEBUFFER_COMPLETE_EXT) destroy();

#endif
            }
   }

// destroy fbo
void volume::destroy()
   {
#ifdef GL_EXT_framebuffer_object

   if (textureId!=0) glDeleteTextures(1, &textureId);
   if (rboId!=0) glDeleteRenderbuffersEXT(1, &rboId);
   if (fboId!=0) glDeleteFramebuffersEXT(1, &fboId);

   textureId=0;
   rboId=0;
   fboId=0;

   HASFBO=FALSE;
   fboWidth=fboHeight=0;

#endif
   }

// check brick size
BOOLINT volume::check(int bricksize,float overmax)
   {
   int border=(int)fceil(overmax);
   return(bricksize>2*border);
   }

// set the volume data
void volume::set_data(unsigned char *data,
                      unsigned char *extra,
                      int width,int height,int depth,
                      float mx,float my,float mz,
                      float sx,float sy,float sz,
                      int bricksize,float overmax)
   {
   int i;

   tileptr *tiles;

   int px,py,pz;

   int border=(int)fceil(overmax);

   float mx2,my2,mz2,
         sx2,sy2,sz2;

   float newsize;

   if (bricksize<=2*border) ERRORMSG();

   for (TILEZ=0,pz=-2*border; pz<depth-1+border; pz+=bricksize-1-2*border,TILEZ++)
      for (TILEY=0,py=-2*border; py<height-1+border; py+=bricksize-1-2*border,TILEY++)
         for (TILEX=0,px=-2*border; px<width-1+border; px+=bricksize-1-2*border,TILEX++)
            {
            sx2=(bricksize-1-2*border)*sx/(width-1);
            sy2=(bricksize-1-2*border)*sy/(height-1);
            sz2=(bricksize-1-2*border)*sz/(depth-1);

            mx2=mx-sx/2.0f+(px+border)*sx/(width-1)+sx2/2.0f;
            my2=my-sy/2.0f+(py+border)*sy/(height-1)+sy2/2.0f;
            mz2=mz-sz/2.0f+(pz+border)*sz/(depth-1)+sz2/2.0f;

            if (TILECNT>=TILEMAX)
               {
               TILEMAX+=TILEINC;
               tiles=new tileptr[TILEMAX];
               for (i=0; i<TILECNT; i++) tiles[i]=TILE[i];
               delete TILE;
               TILE=tiles;
               }

            TILE[TILECNT]=new tile(TFUNC,BASE);

            TILE[TILECNT]->set_data(data,
                                    width,height,depth,
                                    mx2,my2,mz2,
                                    sx2,sy2,sz2,
                                    px,py,pz,
                                    bricksize,border);

            if (extra!=NULL)
               TILE[TILECNT]->set_extra(extra,
                                        width,height,depth,
                                        px,py,pz,
                                        bricksize);

            if (px+bricksize>width+2*border)
               {
               newsize=sx2*(float)(width-1-px)/(bricksize-1-2*border);
               mx2-=(sx2-newsize)/2.0f;
               sx2=newsize;
               }

            if (py+bricksize>height+2*border)
               {
               newsize=sy2*(float)(height-1-py)/(bricksize-1-2*border);
               my2-=(sy2-newsize)/2.0f;
               sy2=newsize;
               }

            if (pz+bricksize>depth+2*border)
               {
               newsize=sz2*(float)(depth-1-pz)/(bricksize-1-2*border);
               mz2-=(sz2-newsize)/2.0f;
               sz2=newsize;
               }

            TILE[TILECNT++]->set_size(mx2,my2,mz2,
                                      sx2,sy2,sz2);
            }

   MX=mx;
   MY=my;
   MZ=mz;

   SX=sx*(width-1+2*border)/(width-1);
   SY=sy*(height-1+2*border)/(height-1);
   SZ=sz*(depth-1+2*border)/(depth-1);

   BX=sx*(width+1)/(width-1);
   BY=sy*(height+1)/(height-1);
   BZ=sz*(depth+1)/(depth-1);

   SLAB=fmin(sx/(width-1),fmin(sy/(height-1),sz/(depth-1)));
   }

// set ambient/diffuse/specular lighting coefficients
void volume::set_light(float noise,float ambnt,float difus,float specl,float specx)
   {
   int i;

   for (i=0; i<TILECNT; i++) TILE[i]->set_light(noise,ambnt,difus,specl,specx);
   }

// sort tiles
void volume::sort(int x,int y,int z,
                  int sx,int sy,int sz,
                  float ex,float ey,float ez,
                  float dx,float dy,float dz,
                  float ux,float uy,float uz,
                  float nearp,float slab,float rslab,
                  BOOLINT lighting)
   {
   tileptr t1,t2;

   if (sx>1)
      {
      t1=TILE[(x+sx/2)+(y+z*TILEY)*TILEX];
      t2=TILE[(x+sx/2-1)+(y+z*TILEY)*TILEX];

      if ((t1->get_mx()+t2->get_mx())/2.0f>ex)
         {
         sort(x+sx/2,y,z,sx-sx/2,sy,sz,ex,ey,ez,dx,dy,dz,ux,uy,uz,nearp,slab,rslab,lighting);
         sort(x,y,z,sx/2,sy,sz,ex,ey,ez,dx,dy,dz,ux,uy,uz,nearp,slab,rslab,lighting);
         }
      else
         {
         sort(x,y,z,sx/2,sy,sz,ex,ey,ez,dx,dy,dz,ux,uy,uz,nearp,slab,rslab,lighting);
         sort(x+sx/2,y,z,sx-sx/2,sy,sz,ex,ey,ez,dx,dy,dz,ux,uy,uz,nearp,slab,rslab,lighting);
         }
      }
   else if (sy>1)
      {
      t1=TILE[x+((y+sy/2)+z*TILEY)*TILEX];
      t2=TILE[x+((y+sy/2-1)+z*TILEY)*TILEX];

      if ((t1->get_my()+t2->get_my())/2.0f>ey)
         {
         sort(x,y+sy/2,z,sx,sy-sy/2,sz,ex,ey,ez,dx,dy,dz,ux,uy,uz,nearp,slab,rslab,lighting);
         sort(x,y,z,sx,sy/2,sz,ex,ey,ez,dx,dy,dz,ux,uy,uz,nearp,slab,rslab,lighting);
         }
      else
         {
         sort(x,y,z,sx,sy/2,sz,ex,ey,ez,dx,dy,dz,ux,uy,uz,nearp,slab,rslab,lighting);
         sort(x,y+sy/2,z,sx,sy-sy/2,sz,ex,ey,ez,dx,dy,dz,ux,uy,uz,nearp,slab,rslab,lighting);
         }
      }
   else if (sz>1)
      {
      t1=TILE[x+(y+(z+sz/2)*TILEY)*TILEX];
      t2=TILE[x+(y+(z+sz/2-1)*TILEY)*TILEX];

      if ((t1->get_mz()+t2->get_mz())/2.0f>ez)
         {
         sort(x,y,z+sz/2,sx,sy,sz-sz/2,ex,ey,ez,dx,dy,dz,ux,uy,uz,nearp,slab,rslab,lighting);
         sort(x,y,z,sx,sy,sz/2,ex,ey,ez,dx,dy,dz,ux,uy,uz,nearp,slab,rslab,lighting);
         }
      else
         {
         sort(x,y,z,sx,sy,sz/2,ex,ey,ez,dx,dy,dz,ux,uy,uz,nearp,slab,rslab,lighting);
         sort(x,y,z+sz/2,sx,sy,sz-sz/2,ex,ey,ez,dx,dy,dz,ux,uy,uz,nearp,slab,rslab,lighting);
         }
      }
   else
      TILE[x+(y+z*TILEY)*TILEX]->render(ex,ey,ez,
                                        dx,dy,dz,
                                        ux,uy,uz,
                                        nearp,slab,rslab,
                                        lighting);
   }

// render the volume
void volume::render(float ex,float ey,float ez,
                    float dx,float dy,float dz,
                    float ux,float uy,float uz,
                    float nearp,float slab,float rslab,
                    BOOLINT lighting)
   {
   // update fbo
   updatefbo();

   // render to fbo
   if (HASFBO && USEFBO)
      {
      glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fboId);

      glClearColor(0,0,0,0);
      glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
      }

   // enable alpha test for pre-multiplied tfs
   if (get_tfunc()->get_premult())
      {
      glAlphaFunc(GL_GREATER,0.0);
      glEnable(GL_ALPHA_TEST);
      }

   // render tiles in back-to-front sorted order
   sort(0,0,0,TILEX,TILEY,TILEZ,
        ex,ey,ez,dx,dy,dz,ux,uy,uz,
        nearp,slab,rslab,
        lighting);

   // disable alpha test for pre-multiplied tfs
   if (get_tfunc()->get_premult())
      glDisable(GL_ALPHA_TEST);

   // render from fbo
   if (HASFBO && USEFBO)
      {
      glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);

      glBindTexture(GL_TEXTURE_2D, textureId);
      glEnable(GL_TEXTURE_2D);

      glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);

      glAlphaFunc(GL_GREATER,0.0);
      glEnable(GL_ALPHA_TEST);

      glBlendFunc(GL_ONE,GL_ONE_MINUS_SRC_ALPHA);
      glEnable(GL_BLEND);

      glMatrixMode(GL_PROJECTION);
      glPushMatrix();
      glLoadIdentity();
      gluOrtho2D(-1.0f,1.0f,-1.0f,1.0f);
      glMatrixMode(GL_MODELVIEW);
      glPushMatrix();
      glLoadIdentity();

      glBegin(GL_QUADS);
      glColor3f(1.0f,1.0f,1.0f);
      glTexCoord2f(0.0f,0.0f);
      glVertex2f(-1.0f,-1.0f);
      glTexCoord2f(1.0f,0.0f);
      glVertex2f(1.0f,-1.0f);
      glTexCoord2f(1.0f,1.0f);
      glVertex2f(1.0f,1.0f);
      glTexCoord2f(0.0f,1.0f);
      glVertex2f(-1.0f,1.0f);
      glEnd();

      glPopMatrix();
      glMatrixMode(GL_PROJECTION);
      glPopMatrix();
      glMatrixMode(GL_MODELVIEW);

      glBindTexture(GL_TEXTURE_2D, 0);
      glDisable(GL_TEXTURE_2D);

      glDisable(GL_ALPHA_TEST);

      glDisable(GL_BLEND);
      }
   }

// draw the surrounding wire frame box
void volume::drawwireframe(float mx,float my,float mz,
                           float sx,float sy,float sz)
   {
   float sx2,sy2,sz2;

   sx2=0.5f*sx;
   sy2=0.5f*sy;
   sz2=0.5f*sz;

   glPushMatrix();
   glTranslatef(mx,my,mz);

   glColor3f(0.5f,0.5f,0.5f);
   glBegin(GL_LINES);
   glVertex3f(-sx2,sy2,-sz2);
   glVertex3f(sx2,sy2,-sz2);
   glVertex3f(-sx2,sy2,-sz2);
   glVertex3f(-sx2,sy2,sz2);
   glVertex3f(sx2,sy2,sz2);
   glVertex3f(-sx2,sy2,sz2);
   glVertex3f(sx2,sy2,sz2);
   glVertex3f(sx2,sy2,-sz2);
   glVertex3f(-sx2,-sy2,-sz2);
   glVertex3f(-sx2,sy2,-sz2);
   glVertex3f(sx2,-sy2,-sz2);
   glVertex3f(sx2,sy2,-sz2);
   glVertex3f(sx2,-sy2,sz2);
   glVertex3f(sx2,sy2,sz2);
   glVertex3f(-sx2,-sy2,sz2);
   glVertex3f(-sx2,sy2,sz2);
   glEnd();

   glDisable(GL_CULL_FACE);
   glColor3f(0.25f,0.25f,0.25f);
   glBegin(GL_TRIANGLE_FAN);
   glVertex3f(-sx2,-sy2,-sz2);
   glVertex3f(sx2,-sy2,-sz2);
   glVertex3f(sx2,-sy2,sz2);
   glVertex3f(-sx2,-sy2,sz2);
   glVertex3f(-sx2,-sy2,-sz2);
   glEnd();
   glEnable(GL_CULL_FACE);

   glPopMatrix();
   }

// use 16-bit fbo
void volume::usefbo(BOOLINT yes)
   {USEFBO=yes;}

// update 16-bit fbo
void volume::updatefbo()
   {
   if (USEFBO)
      {
      GLint viewport[4];
      glGetIntegerv(GL_VIEWPORT,viewport);
      int width=viewport[2];
      int height=viewport[3];

      setup(width,height);
      }
   }

// the volume hierarchy:

mipmap::mipmap(char *base,int res)
   {
   if (res==0) res=256;

   VOLCNT=0;

   TFUNC=new tfunc2D(res);
   HISTO=new histo;

   VOLUME=GRAD=NULL;

   DSX=DSY=DSZ=1.0f;

   GRADMAX=1.0f;

   if (base==NULL) strncpy(BASE,"",MAXSTR);
   else strncpy(BASE,base,MAXSTR);

   strncpy(filestr,"",MAXSTR);
   strncpy(gradstr,"",MAXSTR);
   strncpy(commstr,"",MAXSTR);
   strncpy(zerostr,"",MAXSTR);

   xsflag=FALSE; ysflag=FALSE; zsflag=FALSE;
   xrflag=FALSE; zrflag=FALSE;

   hmvalue=0.0f; hfvalue=0.0f; hsvalue=0.0f;
   knvalue=0;

   CACHE=NULL;

   CSIZEX=0;
   CSIZEY=0;
   CSLICE=0;
   CSLICES=0;

   QUEUEMAX=QUEUEINC;

   QUEUEX=new int[QUEUEMAX];
   QUEUEY=new int[QUEUEMAX];
   QUEUEZ=new int[QUEUEMAX];
   }

mipmap::~mipmap()
   {
   int i;

   for (i=0; i<VOLCNT; i++) delete VOL[i];
   if (VOLCNT>0) delete VOL;

   delete TFUNC;
   delete HISTO;

   if (VOLUME!=NULL) free(VOLUME);
   if (GRAD!=NULL) free(GRAD);

   if (CACHE!=NULL) delete CACHE;

   delete QUEUEX;
   delete QUEUEY;
   delete QUEUEZ;
   }

// reduce a volume to half its size
unsigned char *mipmap::reduce(unsigned char *data,
                              unsigned int width,unsigned int height,unsigned int depth)
   {
   unsigned int i,j,k;

   unsigned char *data2,*ptr;

   if (data==NULL) return(NULL);

   if ((data2=(unsigned char *)malloc((width/2)*(height/2)*(depth/2)))==NULL) ERRORMSG();

   for (ptr=data2,k=0; k<depth-1; k+=2)
      for (j=0; j<height-1; j+=2)
         for (i=0; i<width-1; i+=2)
            *ptr++=((int)data[i+(j+k*height)*width]+
                    (int)data[i+1+(j+k*height)*width]+
                    (int)data[i+(j+1+k*height)*width]+
                    (int)data[i+1+(j+1+k*height)*width]+
                    (int)data[i+(j+(k+1)*height)*width]+
                    (int)data[i+1+(j+(k+1)*height)*width]+
                    (int)data[i+(j+1+(k+1)*height)*width]+
                    (int)data[i+1+(j+1+(k+1)*height)*width]+4)/8;

   return(data2);
   }

// set the volume data
void mipmap::set_data(unsigned char *data,
                      unsigned char *extra,
                      int width,int height,int depth,
                      float mx,float my,float mz,
                      float sx,float sy,float sz,
                      int bricksize,float overmax)
   {
   int i;

   float o;

   unsigned char *data2,*extra2;

   if (VOLCNT!=0)
      {
      for (i=0; i<VOLCNT; i++) delete VOL[i];
      if (VOLCNT>0) delete VOL;
      }

   for (VOLCNT=0,i=bricksize,o=overmax; volume::check(i,o); i/=2,o/=2.0f,VOLCNT++)
      if ((width>>VOLCNT)<=2 || (height>>VOLCNT)<=2 || (depth>>VOLCNT)<=2) break;

   if (VOLCNT==0) ERRORMSG();

   VOL=new volumeptr[VOLCNT];

   VOL[0]=new volume(TFUNC,BASE);

   VOL[0]->set_data(data,
                    extra,
                    width,height,depth,
                    mx,my,mz,
                    sx,sy,sz,
                    bricksize,overmax);

   for (i=1; i<VOLCNT; i++)
      {
      VOL[i]=new volume(TFUNC,BASE);

      data2=reduce(data,width,height,depth);
      extra2=reduce(extra,width,height,depth);

      width/=2;
      height/=2;
      depth/=2;

      bricksize/=2;
      overmax/=2.0f;

      VOL[i]->set_data(data2,
                       extra2,
                       width,height,depth,
                       mx,my,mz,
                       sx,sy,sz,
                       bricksize,overmax);

      if (i>1)
         {
         free(data);
         if (extra!=NULL) free(extra);
         }

      data=data2;
      extra=extra2;
      }

   if (VOLCNT>1)
      {
      free(data);
      if (extra!=NULL) free(extra);
      }
   }

// swap the volume along the axis
unsigned char *mipmap::swap(unsigned char *data,
                            unsigned int *width,unsigned int *height,unsigned int *depth,
                            float *dsx,float *dsy,float *dsz,
                            BOOLINT xswap,BOOLINT yswap,BOOLINT zswap,
                            BOOLINT xrotate,BOOLINT zrotate)
   {
   unsigned int i,j,k;

   unsigned char *data2,*ptr;

   unsigned int size;
   float dim;

   if (xrotate)
      {
      if ((data2=(unsigned char *)malloc((*width)*(*height)*(*depth)))==NULL) ERRORMSG();

      for (ptr=data2,k=0; k<*depth; k++)
         for (j=0; j<*width; j++)
            for (i=0; i<*height; i++)
               *ptr++=data[j+(i+k*(*height))*(*width)];

      size=*width;
      *width=*height;
      *height=size;

      if (dsx!=NULL && dsy!=NULL && dsz!=NULL)
         {
         dim=*dsx;
         *dsx=*dsy;
         *dsy=dim;
         }

      free(data);
      data=data2;
      }

   if (zrotate)
      {
      if ((data2=(unsigned char *)malloc((*width)*(*height)*(*depth)))==NULL) ERRORMSG();

      for (ptr=data2,k=0; k<*height; k++)
         for (j=0; j<*depth; j++)
            for (i=0; i<*width; i++)
               *ptr++=data[i+(k+j*(*height))*(*width)];

      size=*height;
      *height=*depth;
      *depth=size;

      if (dsx!=NULL && dsy!=NULL && dsz!=NULL)
         {
         dim=*dsy;
         *dsy=*dsz;
         *dsz=dim;
         }

      free(data);
      data=data2;
      }

   if (xswap)
      {
      if ((data2=(unsigned char *)malloc((*width)*(*height)*(*depth)))==NULL) ERRORMSG();

      for (ptr=data2,k=0; k<*depth; k++)
         for (j=0; j<*height; j++)
            for (i=0; i<*width; i++)
               *ptr++=data[*width-1-i+(j+k*(*height))*(*width)];

      free(data);
      data=data2;
      }

   if (yswap)
      {
      if ((data2=(unsigned char *)malloc((*width)*(*height)*(*depth)))==NULL) ERRORMSG();

      for (ptr=data2,k=0; k<*depth; k++)
         for (j=0; j<*height; j++)
            for (i=0; i<*width; i++)
               *ptr++=data[i+(*height-1-j+k*(*height))*(*width)];

      free(data);
      data=data2;
      }

   if (zswap)
      {
      if ((data2=(unsigned char *)malloc((*width)*(*height)*(*depth)))==NULL) ERRORMSG();

      for (ptr=data2,k=0; k<(*depth); k++)
         for (j=0; j<(*height); j++)
            for (i=0; i<(*width); i++)
               *ptr++=data[i+(j+(*depth-1-k)*(*height))*(*width)];

      free(data);
      data=data2;
      }

   return(data);
   }

// cache a row of slices
void mipmap::cache(const unsigned char *data,
                   unsigned int width,unsigned int height,unsigned int depth,
                   int slice,int slices)
   {
   int i;

   if (slices!=CSLICES)
      {
      if (CACHE!=NULL) delete CACHE;
      CACHE=NULL;

      if (slices>0)
         {
         CACHE=new unsigned char[width*height*(unsigned int)slices];

         for (i=0; i<slices; i++)
            if (slice+i>=0 && slice+i<(int)depth)
               memcpy(&CACHE[width*height*i],&data[width*height*(unsigned int)(slice+i)],width*height);
         }

      CSIZEX=width;
      CSIZEY=height;
      CSLICE=slice;
      CSLICES=slices;
      }

   if (CSLICES>0)
      {
      if (slice>CSLICE)
         for (i=0; i<CSLICES; i++)
            if (slice+i>=0 && slice+i<(int)depth)
               if (slice+i>=CSLICE && slice+i<CSLICE+CSLICES)
                  memcpy(&CACHE[CSIZEX*CSIZEY*i],&CACHE[CSIZEX*CSIZEY*(i+slice-CSLICE)],CSIZEX*CSIZEY);

      if (slice<CSLICE)
         for (i=CSLICES-1; i>=0; i--)
            if (slice+i>=0 && slice+i<(int)depth)
               if (slice+i>=CSLICE && slice+i<CSLICE+CSLICES)
                  memcpy(&CACHE[CSIZEX*CSIZEY*i],&CACHE[CSIZEX*CSIZEY*(i+CSLICE-slice)],CSIZEX*CSIZEY);

      for (i=0; i<CSLICES; i++)
         if (slice+i>=0 && slice+i<(int)depth)
            if (slice+i<CSLICE || slice+i>=CSLICE+CSLICES)
               memcpy(&CACHE[CSIZEX*CSIZEY*i],&data[CSIZEX*CSIZEY*(unsigned int)(slice+i)],CSIZEX*CSIZEY);

      CSLICE=slice;
      }
   }

inline unsigned char mipmap::get(const unsigned char *data,
                                 const unsigned int width,const unsigned int height,const unsigned int depth,
                                 const unsigned int x,const unsigned int y,const unsigned int z)
   {
   if (CACHE!=NULL)
      if ((int)z>=CSLICE && (int)z<CSLICE+CSLICES)
         return(CACHE[x+(y+(z-CSLICE)*height)*width]);

   return(data[x+(y+z*height)*width]);
   }

inline void mipmap::set(unsigned char *data,
                        const unsigned int width,const unsigned int height,const unsigned int depth,
                        const unsigned int x,const unsigned int y,const unsigned int z,unsigned char v)
   {data[x+(y+z*height)*width]=v;}

// calculate the gradient magnitude
unsigned char *mipmap::gradmag(unsigned char *data,
                               unsigned int width,unsigned int height,unsigned int depth,
                               float dsx,float dsy,float dsz,
                               float *gradmax)
   {
   static const float mingrad=0.1f;

   int i,j,k;

   unsigned char *data2,*ptr;

   float gx,gy,gz;
   float gm,gmax;

   float minds;

   minds=fmin(dsx,fmin(dsy,dsz));

   dsx/=minds;
   dsy/=minds;
   dsz/=minds;

   if (dsx>4.0f) dsx=4.0f;
   else if (dsx>2.0f) dsx=2.0f;
   else if (dsx>1.0f) dsx=1.0f;

   if (dsy>4.0f) dsy=4.0f;
   else if (dsy>2.0f) dsy=2.0f;
   else if (dsy>1.0f) dsy=1.0f;

   if (dsz>4.0f) dsz=4.0f;
   else if (dsz>2.0f) dsz=2.0f;
   else if (dsz>1.0f) dsz=1.0f;

   dsx=1.0f/dsx;
   dsy=1.0f/dsy;
   dsz=1.0f/dsz;

   if ((data2=(unsigned char *)malloc(width*height*depth))==NULL) ERRORMSG();

   for (gmax=1.0f,k=0; k<(int)depth; k+=2)
      for (j=0; j<(int)height; j+=2)
         for (i=0; i<(int)width; i+=2)
            {
            if (i>0)
               if (i<(int)width-1) gx=(get(data,width,height,depth,i+1,j,k)-get(data,width,height,depth,i-1,j,k))/2.0f;
               else gx=get(data,width,height,depth,i,j,k)-get(data,width,height,depth,i-1,j,k);
            else gx=get(data,width,height,depth,i+1,j,k)-get(data,width,height,depth,i,j,k);

            if (j>0)
               if (j<(int)height-1) gy=(get(data,width,height,depth,i,j+1,k)-get(data,width,height,depth,i,j-1,k))/2.0f;
               else gy=get(data,width,height,depth,i,j,k)-get(data,width,height,depth,i,j-1,k);
            else gy=get(data,width,height,depth,i,j+1,k)-get(data,width,height,depth,i,j,k);

            if (k>0)
               if (k<(int)depth-1) gz=(get(data,width,height,depth,i,j,k+1)-get(data,width,height,depth,i,j,k-1))/2.0f;
               else gz=get(data,width,height,depth,i,j,k)-get(data,width,height,depth,i,j,k-1);
            else gz=get(data,width,height,depth,i,j,k+1)-get(data,width,height,depth,i,j,k);

            gm=fsqr(gx*dsx)+fsqr(gy*dsy)+fsqr(gz*dsz);
            if (gm>gmax) gmax=gm;
            }

   for (ptr=data2,k=0; k<(int)depth; k++)
      for (j=0; j<(int)height; j++)
         for (i=0; i<(int)width; i++)
            {
            if (i>0)
               if (i<(int)width-1) gx=(get(data,width,height,depth,i+1,j,k)-get(data,width,height,depth,i-1,j,k))/2.0f;
               else gx=get(data,width,height,depth,i,j,k)-get(data,width,height,depth,i-1,j,k);
            else gx=get(data,width,height,depth,i+1,j,k)-get(data,width,height,depth,i,j,k);

            if (j>0)
               if (j<(int)height-1) gy=(get(data,width,height,depth,i,j+1,k)-get(data,width,height,depth,i,j-1,k))/2.0f;
               else gy=get(data,width,height,depth,i,j,k)-get(data,width,height,depth,i,j-1,k);
            else gy=get(data,width,height,depth,i,j+1,k)-get(data,width,height,depth,i,j,k);

            if (k>0)
               if (k<(int)depth-1) gz=(get(data,width,height,depth,i,j,k+1)-get(data,width,height,depth,i,j,k-1))/2.0f;
               else gz=get(data,width,height,depth,i,j,k)-get(data,width,height,depth,i,j,k-1);
            else gz=get(data,width,height,depth,i,j,k+1)-get(data,width,height,depth,i,j,k);

            *ptr++=ftrc(255.0f*threshold(fsqrt(fmin((fsqr(gx*dsx)+fsqr(gy*dsy)+fsqr(gz*dsz))/gmax,1.0f)),mingrad)+0.5f);
            }

   if (gradmax!=NULL) *gradmax=fsqrt(gmax)/255.0f;

   return(data2);
   }

inline float mipmap::getgrad(unsigned char *data,
                             int width,int height,int depth,
                             int i,int j,int k,
                             float dsx,float dsy,float dsz)
   {
   unsigned char *ptr;
   int v;

   float gx,gy,gz;

   ptr=&data[i+(j+k*height)*width];
   v=*ptr;

   if (i>0) gx=v-ptr[-1];
   else gx=ptr[1]-v;

   if (j>0) gy=v-ptr[-width];
   else gy=ptr[width]-v;

   if (k>0) gz=v-ptr[-width*height];
   else gz=ptr[width*height]-v;

   return(fsqrt(fsqr(gx*dsx)+fsqr(gy*dsy)+fsqr(gz*dsz)));
   }

inline float mipmap::getgrad2(unsigned char *data,
                              int width,int height,int depth,
                              int i,int j,int k,
                              float dsx,float dsy,float dsz)
   {
   unsigned char *ptr;
   int v;

   float gx,gy,gz;

   ptr=&data[i+(j+k*height)*width];
   v=*ptr;

   if (i>0)
      if (i<width-1) gx=0.5f*(ptr[1]-ptr[-1]);
      else gx=v-ptr[-1];
   else gx=ptr[1]-v;

   if (j>0)
      if (j<height-1) gy=0.5f*(ptr[width]-ptr[-width]);
      else gy=v-ptr[-width];
   else gy=ptr[width]-v;

   if (k>0)
      if (k<depth-1) gz=0.5f*(ptr[width*height]-ptr[-width*height]);
      else gz=v-ptr[-width*height];
   else gz=ptr[width*height]-v;

   return(fsqrt(fsqr(gx*dsx)+fsqr(gy*dsy)+fsqr(gz*dsz)));
   }

inline float mipmap::getsobel(unsigned char *data,
                              int width,int height,int depth,
                              int i,int j,int k,
                              float dsx,float dsy,float dsz)
   {
   unsigned char *ptr;
   int v0,v[27];

   float gx,gy,gz;

   if (i>0 && i<width-1 && j>0 && j<height-1 && k>0 && k<depth-1)
      {
      ptr=&data[i-1+(j-1+(k-1)*height)*width];

      v[0]=*ptr++;
      v[1]=3*(*ptr++);
      v[2]=*ptr;
      ptr+=width-2;
      v[3]=3*(*ptr++);
      v[4]=6*(*ptr++);
      v[5]=3*(*ptr);
      ptr+=width-2;
      v[6]=*ptr++;
      v[7]=3*(*ptr++);
      v[8]=*ptr;
      ptr+=width*height-2*width-2;
      v[9]=3*(*ptr++);
      v[10]=6*(*ptr++);
      v[11]=3*(*ptr);
      ptr+=width-2;
      v[12]=6*(*ptr++);
      v[13]=*ptr++;
      v[14]=6*(*ptr);
      ptr+=width-2;
      v[15]=3*(*ptr++);
      v[16]=6*(*ptr++);
      v[17]=3*(*ptr);
      ptr+=width*height-2*width-2;
      v[18]=*ptr++;
      v[19]=3*(*ptr++);
      v[20]=*ptr;
      ptr+=width-2;
      v[21]=3*(*ptr++);
      v[22]=6*(*ptr++);
      v[23]=3*(*ptr);
      ptr+=width-2;
      v[24]=*ptr++;
      v[25]=3*(*ptr++);
      v[26]=*ptr;

      gx=(-v[0]-v[3]-v[6]-v[9]-v[12]-v[15]-v[18]-v[21]-v[24]+v[2]+v[5]+v[8]+v[11]+v[14]+v[17]+v[20]+v[23]+v[26])/44.0f;
      gy=(-v[0]-v[1]-v[2]-v[9]-v[10]-v[11]-v[18]-v[19]-v[20]+v[6]+v[7]+v[8]+v[15]+v[16]+v[17]+v[24]+v[25]+v[26])/44.0f;
      gz=(-v[0]-v[1]-v[2]-v[3]-v[4]-v[5]-v[6]-v[7]-v[8]+v[18]+v[19]+v[20]+v[21]+v[22]+v[23]+v[24]+v[25]+v[26])/44.0f;
      }
   else
      {
      ptr=&data[i+(j+k*height)*width];
      v0=*ptr;

      if (i>0)
         if (i<width-1) gx=0.5f*(ptr[1]-ptr[-1]);
         else gx=v0-ptr[-1];
      else gx=ptr[1]-v0;

      if (j>0)
         if (j<height-1) gy=0.5f*(ptr[width]-ptr[-width]);
         else gy=v0-ptr[-width];
      else gy=ptr[width]-v0;

      if (k>0)
         if (k<depth-1) gz=0.5f*(ptr[width*height]-ptr[-width*height]);
         else gz=v0-ptr[-width*height];
      else gz=ptr[width*height]-v0;
      }

   return(fsqrt(fsqr(gx*dsx)+fsqr(gy*dsy)+fsqr(gz*dsz)));
   }

inline float mipmap::threshold(float x,float thres)
   {
   if (x>=thres) return(x);
   return(x*fexp(-3.0f*fsqr((thres-x)/thres)));
   }

// calculate the gradient magnitude with multi-level averaging
unsigned char *mipmap::gradmagML(unsigned char *data,
                                 unsigned int width,unsigned int height,unsigned int depth,
                                 float dsx,float dsy,float dsz,
                                 float *gradmax)
   {
   static const int maxlevel=2;
   static const float mingrad=0.1f;

   int i,j,k;

   float *data2,*ptr2;
   float *data3,*ptr3;
   unsigned char *data4,*ptr4;
   unsigned char *data5,*ptr5;

   int width2,height2,depth2;

   float gm,gmax,gmax2;

   int level;
   float weight;

   float minds;

   minds=fmin(dsx,fmin(dsy,dsz));

   dsx/=minds;
   dsy/=minds;
   dsz/=minds;

   if (dsx>4.0f) dsx=4.0f;
   else if (dsx>2.0f) dsx=2.0f;
   else if (dsx>1.0f) dsx=1.0f;

   if (dsy>4.0f) dsy=4.0f;
   else if (dsy>2.0f) dsy=2.0f;
   else if (dsy>1.0f) dsy=1.0f;

   if (dsz>4.0f) dsz=4.0f;
   else if (dsz>2.0f) dsz=2.0f;
   else if (dsz>1.0f) dsz=1.0f;

   dsx=1.0f/dsx;
   dsy=1.0f/dsy;
   dsz=1.0f/dsz;

   if ((data2=(float *)malloc(width*height*depth*sizeof(float)))==NULL) ERRORMSG();

   gmax=0.0f;

   for (ptr2=data2,k=0; k<(int)depth; k++)
      for (j=0; j<(int)height; j++)
         for (i=0; i<(int)width; i++)
            {
#ifndef SOBEL
            gm=getgrad(data,width,height,depth,i,j,k,dsx,dsy,dsz);
#else
            gm=getsobel(data,width,height,depth,i,j,k,dsx,dsy,dsz);
#endif
            if (gm>gmax) gmax=gm;

            *ptr2++=gm;
            }

   if (gmax==0.0f) gmax=1.0f;

   for (ptr2=data2,k=0; k<(int)depth; k++)
      for (j=0; j<(int)height; j++)
         for (i=0; i<(int)width; i++)
            {
            gm=*ptr2;
            *ptr2++=threshold(gm/gmax,mingrad);
            }

   width2=width;
   height2=height;
   depth2=depth;

   data4=ptr4=data;

   level=0;
   weight=1.0f;

   gmax2=0.0f;

   while (width2>5 && height2>5 && depth2>5 && level<maxlevel)
      {
      data5=reduce(data4,width2,height2,depth2);
      if (data4!=data) free(data4);
      data4=data5;

      width2=width2/2;
      height2=height2/2;
      depth2=depth2/2;

      level++;
      weight*=0.5f;

      if ((data3=(float *)malloc(width2*height2*depth2*sizeof(float)))==NULL) ERRORMSG();

      for (ptr3=data3,k=0; k<depth2; k++)
         for (j=0; j<height2; j++)
            for (i=0; i<width2; i++)
            {
#ifndef SOBEL
            gm=getgrad(data4,width2,height2,depth2,i,j,k,dsx,dsy,dsz);
#else
            gm=getsobel(data4,width2,height2,depth2,i,j,k,dsx,dsy,dsz);
#endif
            *ptr3++=gm/gmax;
            }

      for (ptr2=data2,k=0; k<(int)depth; k++)
         for (j=0; j<(int)height; j++)
            for (i=0; i<(int)width; i++)
               {
               gm=*ptr2;
               gm+=weight*threshold(getscalar(data3,width2,height2,depth2,(float)i/(width-1),(float)j/(height-1),(float)k/(depth-1)),mingrad);
               if (gm>gmax2) gmax2=gm;

               *ptr2++=gm;
               }

      free(data3);
      }

   if (gmax2==0.0f) gmax2=1.0f;

   if (data4!=data) free(data4);

   if ((data5=(unsigned char *)malloc(width*height*depth))==NULL) ERRORMSG();

   for (ptr2=data2,ptr5=data5,k=0; k<(int)depth; k++)
      for (j=0; j<(int)height; j++)
         for (i=0; i<(int)width; i++)
            {
            gm=*ptr2++;
            *ptr5++=ftrc(255.0f*gm/gmax2+0.5f);
            }

   free(data2);

   if (gradmax!=NULL) *gradmax=gmax2/255.0f;

   return(data5);
   }

// calculate the variance
unsigned char *mipmap::variance(unsigned char *data,
                                unsigned int width,unsigned int height,unsigned int depth)
   {
   int i,j,k;

   unsigned char *data2,*ptr;

   int val,cnt;

   int dev,dmax;

   if ((data2=(unsigned char *)malloc(width*height*depth))==NULL) ERRORMSG();

   for (dmax=1,k=0; k<(int)depth; k+=2)
      for (j=0; j<(int)height; j+=2)
         for (i=0; i<(int)width; i+=2)
            {
            // central voxel
            val=get(data,width,height,depth,i,j,k);
            dev=cnt=0;

            // x-axis
            if (i>0) {dev+=abs(get(data,width,height,depth,i-1,j,k)-val); cnt++;}
            if (i<(int)width-1) {dev+=abs(get(data,width,height,depth,i+1,j,k)-val); cnt++;}

            // y-axis
            if (j>0) {dev+=abs(get(data,width,height,depth,i,j-1,k)-val); cnt++;}
            if (j<(int)height-1) {dev+=abs(get(data,width,height,depth,i,j+1,k)-val); cnt++;}

            // z-axis
            if (k>0) {dev+=abs(get(data,width,height,depth,i,j,k-1)-val); cnt++;}
            if (k<(int)depth-1) {dev+=abs(get(data,width,height,depth,i,j,k+1)-val); cnt++;}

            // xy-plane
            if (i>0 && j>0) {dev+=abs(get(data,width,height,depth,i-1,j-1,k)-val); cnt++;}
            if (i<(int)width-1 && j>0) {dev+=abs(get(data,width,height,depth,i+1,j-1,k)-val); cnt++;}
            if (i>0 && j<(int)height-1) {dev+=abs(get(data,width,height,depth,i-1,j+1,k)-val); cnt++;}
            if (i<(int)width-1 && j<(int)height-1) {dev+=abs(get(data,width,height,depth,i+1,j+1,k)-val); cnt++;}

            // xz-plane
            if (i>0 && k>0) {dev+=abs(get(data,width,height,depth,i-1,j,k-1)-val); cnt++;}
            if (i<(int)width-1 && k>0) {dev+=abs(get(data,width,height,depth,i+1,j,k-1)-val); cnt++;}
            if (i>0 && k<(int)depth-1) {dev+=abs(get(data,width,height,depth,i-1,j,k+1)-val); cnt++;}
            if (i<(int)width-1 && k<(int)depth-1) {dev+=abs(get(data,width,height,depth,i+1,j,k+1)-val); cnt++;}

            // yz-plane
            if (j>0 && k>0) {dev+=abs(get(data,width,height,depth,i,j-1,k-1)-val); cnt++;}
            if (j<(int)height-1 && k>0) {dev+=abs(get(data,width,height,depth,i,j+1,k-1)-val); cnt++;}
            if (j>0 && k<(int)depth-1) {dev+=abs(get(data,width,height,depth,i,j-1,k+1)-val); cnt++;}
            if (j<(int)height-1 && k<(int)depth-1) {dev+=abs(get(data,width,height,depth,i,j+1,k+1)-val); cnt++;}

            // bottom xy-plane
            if (i>0 && j>0 && k>0) {dev+=abs(get(data,width,height,depth,i-1,j-1,k-1)-val); cnt++;}
            if (i<(int)width-1 && j>0 && k>0) {dev+=abs(get(data,width,height,depth,i+1,j-1,k-1)-val); cnt++;}
            if (i>0 && j<(int)height-1 && k>0) {dev+=abs(get(data,width,height,depth,i-1,j+1,k-1)-val); cnt++;}
            if (i<(int)width-1 && j<(int)height-1 && k>0) {dev+=abs(get(data,width,height,depth,i+1,j+1,k-1)-val); cnt++;}

            // top xy-plane
            if (i>0 && j>0 && k<(int)depth-1) {dev+=abs(get(data,width,height,depth,i-1,j-1,k+1)-val); cnt++;}
            if (i<(int)width-1 && j>0 && k<(int)depth-1) {dev+=abs(get(data,width,height,depth,i+1,j-1,k+1)-val); cnt++;}
            if (i>0 && j<(int)height-1 && k<(int)depth-1) {dev+=abs(get(data,width,height,depth,i-1,j+1,k+1)-val); cnt++;}
            if (i<(int)width-1 && j<(int)height-1 && k<(int)depth-1) {dev+=abs(get(data,width,height,depth,i+1,j+1,k+1)-val); cnt++;}

            dev=(dev+cnt/2)/cnt;
            if (dev>dmax) dmax=dev;
            }

   for (ptr=data2,k=0; k<(int)depth; k++)
      for (j=0; j<(int)height; j++)
         for (i=0; i<(int)width; i++)
            {
            // central voxel
            val=get(data,width,height,depth,i,j,k);
            dev=cnt=0;

            // x-axis
            if (i>0) {dev+=abs(get(data,width,height,depth,i-1,j,k)-val); cnt++;}
            if (i<(int)width-1) {dev+=abs(get(data,width,height,depth,i+1,j,k)-val); cnt++;}

            // y-axis
            if (j>0) {dev+=abs(get(data,width,height,depth,i,j-1,k)-val); cnt++;}
            if (j<(int)height-1) {dev+=abs(get(data,width,height,depth,i,j+1,k)-val); cnt++;}

            // z-axis
            if (k>0) {dev+=abs(get(data,width,height,depth,i,j,k-1)-val); cnt++;}
            if (k<(int)depth-1) {dev+=abs(get(data,width,height,depth,i,j,k+1)-val); cnt++;}

            // xy-plane
            if (i>0 && j>0) {dev+=abs(get(data,width,height,depth,i-1,j-1,k)-val); cnt++;}
            if (i<(int)width-1 && j>0) {dev+=abs(get(data,width,height,depth,i+1,j-1,k)-val); cnt++;}
            if (i>0 && j<(int)height-1) {dev+=abs(get(data,width,height,depth,i-1,j+1,k)-val); cnt++;}
            if (i<(int)width-1 && j<(int)height-1) {dev+=abs(get(data,width,height,depth,i+1,j+1,k)-val); cnt++;}

            // xz-plane
            if (i>0 && k>0) {dev+=abs(get(data,width,height,depth,i-1,j,k-1)-val); cnt++;}
            if (i<(int)width-1 && k>0) {dev+=abs(get(data,width,height,depth,i+1,j,k-1)-val); cnt++;}
            if (i>0 && k<(int)depth-1) {dev+=abs(get(data,width,height,depth,i-1,j,k+1)-val); cnt++;}
            if (i<(int)width-1 && k<(int)depth-1) {dev+=abs(get(data,width,height,depth,i+1,j,k+1)-val); cnt++;}

            // yz-plane
            if (j>0 && k>0) {dev+=abs(get(data,width,height,depth,i,j-1,k-1)-val); cnt++;}
            if (j<(int)height-1 && k>0) {dev+=abs(get(data,width,height,depth,i,j+1,k-1)-val); cnt++;}
            if (j>0 && k<(int)depth-1) {dev+=abs(get(data,width,height,depth,i,j-1,k+1)-val); cnt++;}
            if (j<(int)height-1 && k<(int)depth-1) {dev+=abs(get(data,width,height,depth,i,j+1,k+1)-val); cnt++;}

            // bottom xy-plane
            if (i>0 && j>0 && k>0) {dev+=abs(get(data,width,height,depth,i-1,j-1,k-1)-val); cnt++;}
            if (i<(int)width-1 && j>0 && k>0) {dev+=abs(get(data,width,height,depth,i+1,j-1,k-1)-val); cnt++;}
            if (i>0 && j<(int)height-1 && k>0) {dev+=abs(get(data,width,height,depth,i-1,j+1,k-1)-val); cnt++;}
            if (i<(int)width-1 && j<(int)height-1 && k>0) {dev+=abs(get(data,width,height,depth,i+1,j+1,k-1)-val); cnt++;}

            // top xy-plane
            if (i>0 && j>0 && k<(int)depth-1) {dev+=abs(get(data,width,height,depth,i-1,j-1,k+1)-val); cnt++;}
            if (i<(int)width-1 && j>0 && k<(int)depth-1) {dev+=abs(get(data,width,height,depth,i+1,j-1,k+1)-val); cnt++;}
            if (i>0 && j<(int)height-1 && k<(int)depth-1) {dev+=abs(get(data,width,height,depth,i-1,j+1,k+1)-val); cnt++;}
            if (i<(int)width-1 && j<(int)height-1 && k<(int)depth-1) {dev+=abs(get(data,width,height,depth,i+1,j+1,k+1)-val); cnt++;}

            dev=(dev+cnt/2)/cnt;
            *ptr++=ftrc(255.0f*fmin((float)dev/dmax,1.0f)+0.5f);
            }

   return(data2);
   }

// blur the volume
void mipmap::blur(unsigned char *data,
                  unsigned int width,unsigned int height,unsigned int depth)
   {
   int i,j,k;

   unsigned char *ptr;

   int val,cnt;

   for (ptr=data,k=0; k<(int)depth; k++)
      {
      cache(data,width,height,depth,k-1,2);

      for (j=0; j<(int)height; j++)
         for (i=0; i<(int)width; i++)
            {
            // central voxel
            val=54*get(data,width,height,depth,i,j,k);
            cnt=54;

            // x-axis
            if (i>0) {val+=6*get(data,width,height,depth,i-1,j,k); cnt+=6;}
            if (i<(int)width-1) {val+=6*get(data,width,height,depth,i+1,j,k); cnt+=6;}

            // y-axis
            if (j>0) {val+=6*get(data,width,height,depth,i,j-1,k); cnt+=6;}
            if (j<(int)height-1) {val+=6*get(data,width,height,depth,i,j+1,k); cnt+=6;}

            // z-axis
            if (k>0) {val+=6*get(data,width,height,depth,i,j,k-1); cnt+=6;}
            if (k<(int)depth-1) {val+=6*get(data,width,height,depth,i,j,k+1); cnt+=6;}

            // xy-plane
            if (i>0 && j>0) {val+=2*get(data,width,height,depth,i-1,j-1,k); cnt+=2;}
            if (i<(int)width-1 && j>0) {val+=2*get(data,width,height,depth,i+1,j-1,k); cnt+=2;}
            if (i>0 && j<(int)height-1) {val+=2*get(data,width,height,depth,i-1,j+1,k); cnt+=2;}
            if (i<(int)width-1 && j<(int)height-1) {val+=2*get(data,width,height,depth,i+1,j+1,k); cnt+=2;}

            // xz-plane
            if (i>0 && k>0) {val+=2*get(data,width,height,depth,i-1,j,k-1); cnt+=2;}
            if (i<(int)width-1 && k>0) {val+=2*get(data,width,height,depth,i+1,j,k-1); cnt+=2;}
            if (i>0 && k<(int)depth-1) {val+=2*get(data,width,height,depth,i-1,j,k+1); cnt+=2;}
            if (i<(int)width-1 && k<(int)depth-1) {val+=2*get(data,width,height,depth,i+1,j,k+1); cnt+=2;}

            // yz-plane
            if (j>0 && k>0) {val+=2*get(data,width,height,depth,i,j-1,k-1); cnt+=2;}
            if (j<(int)height-1 && k>0) {val+=2*get(data,width,height,depth,i,j+1,k-1); cnt+=2;}
            if (j>0 && k<(int)depth-1) {val+=2*get(data,width,height,depth,i,j-1,k+1); cnt+=2;}
            if (j<(int)height-1 && k<(int)depth-1) {val+=2*get(data,width,height,depth,i,j+1,k+1); cnt+=2;}

            // bottom xy-plane
            if (i>0 && j>0 && k>0) {val+=get(data,width,height,depth,i-1,j-1,k-1); cnt++;}
            if (i<(int)width-1 && j>0 && k>0) {val+=get(data,width,height,depth,i+1,j-1,k-1); cnt++;}
            if (i>0 && j<(int)height-1 && k>0) {val+=get(data,width,height,depth,i-1,j+1,k-1); cnt++;}
            if (i<(int)width-1 && j<(int)height-1 && k>0) {val+=get(data,width,height,depth,i+1,j+1,k-1); cnt++;}

            // top xy-plane
            if (i>0 && j>0 && k<(int)depth-1) {val+=get(data,width,height,depth,i-1,j-1,k+1); cnt++;}
            if (i<(int)width-1 && j>0 && k<(int)depth-1) {val+=get(data,width,height,depth,i+1,j-1,k+1); cnt++;}
            if (i>0 && j<(int)height-1 && k<(int)depth-1) {val+=get(data,width,height,depth,i-1,j+1,k+1); cnt++;}
            if (i<(int)width-1 && j<(int)height-1 && k<(int)depth-1) {val+=get(data,width,height,depth,i+1,j+1,k+1); cnt++;}

            *ptr++=(val+cnt/2)/cnt;
            }
      }

   cache();
   }

// set gradient to maximum where transfer function is transparent
void mipmap::usetf(unsigned char *data,unsigned char *grad,
                   unsigned int width,unsigned int height,unsigned int depth)
   {
   int i,j,k;

   unsigned char *ptr1,*ptr2;

   int mindata,maxdata;

   int val,nbg[26];

   int oldmode;

   oldmode=TFUNC->get_mode();

   if (!TFUNC->get_imode()) TFUNC->set_mode(0);

   TFUNC->preint(TRUE);

   for (ptr1=data,ptr2=grad,k=0; k<(int)depth; k++)
      for (j=0; j<(int)height; j++)
         for (i=0; i<(int)width; i++,ptr1++,ptr2++)
            {
            // central voxel
            mindata=maxdata=*ptr1;

            // x-axis:

            if (i>0) nbg[0]=get(data,width,height,depth,i-1,j,k); else nbg[0]=*ptr1;
            if (i<(int)width-1) nbg[1]=get(data,width,height,depth,i+1,j,k); else nbg[1]=*ptr1;

            val=(*ptr1)+nbg[0]; if (val/2<mindata) mindata=val/2; else if ((val+1)/2>maxdata) maxdata=(val+1)/2;
            val=(*ptr1)+nbg[1]; if (val/2<mindata) mindata=val/2; else if ((val+1)/2>maxdata) maxdata=(val+1)/2;

            // y-axis:

            if (j>0) nbg[2]=get(data,width,height,depth,i,j-1,k); else nbg[2]=*ptr1;
            if (j<(int)height-1) nbg[3]=get(data,width,height,depth,i,j+1,k); else nbg[3]=*ptr1;

            val=(*ptr1)+nbg[2]; if (val/2<mindata) mindata=val/2; else if ((val+1)/2>maxdata) maxdata=(val+1)/2;
            val=(*ptr1)+nbg[3]; if (val/2<mindata) mindata=val/2; else if ((val+1)/2>maxdata) maxdata=(val+1)/2;

            // z-axis:

            if (k>0) nbg[4]=get(data,width,height,depth,i,j,k-1); else nbg[4]=*ptr1;
            if (k<(int)depth-1) nbg[5]=get(data,width,height,depth,i,j,k+1); else nbg[5]=*ptr1;

            val=(*ptr1)+nbg[4]; if (val/2<mindata) mindata=val/2; else if ((val+1)/2>maxdata) maxdata=(val+1)/2;
            val=(*ptr1)+nbg[5]; if (val/2<mindata) mindata=val/2; else if ((val+1)/2>maxdata) maxdata=(val+1)/2;

            // xy-plane:

            if (i>0 && j>0) nbg[6]=get(data,width,height,depth,i-1,j-1,k); else nbg[6]=*ptr1;
            if (i<(int)width-1 && j>0) nbg[7]=get(data,width,height,depth,i+1,j-1,k); else nbg[7]=*ptr1;
            if (i>0 && j<(int)height-1) nbg[8]=get(data,width,height,depth,i-1,j+1,k); else nbg[8]=*ptr1;
            if (i<(int)width-1 && j<(int)height-1) nbg[9]=get(data,width,height,depth,i+1,j+1,k); else nbg[9]=*ptr1;

            val=(*ptr1)+nbg[0]+nbg[2]+nbg[6]; if (val/4<mindata) mindata=val/4; else if ((val+3)/4>maxdata) maxdata=(val+3)/4;
            val=(*ptr1)+nbg[1]+nbg[2]+nbg[7]; if (val/4<mindata) mindata=val/4; else if ((val+3)/4>maxdata) maxdata=(val+3)/4;
            val=(*ptr1)+nbg[0]+nbg[3]+nbg[8]; if (val/4<mindata) mindata=val/4; else if ((val+3)/4>maxdata) maxdata=(val+3)/4;
            val=(*ptr1)+nbg[1]+nbg[3]+nbg[9]; if (val/4<mindata) mindata=val/4; else if ((val+3)/4>maxdata) maxdata=(val+3)/4;

            // xz-plane:

            if (i>0 && k>0) nbg[10]=get(data,width,height,depth,i-1,j,k-1); else nbg[10]=*ptr1;
            if (i<(int)width-1 && k>0) nbg[11]=get(data,width,height,depth,i+1,j,k-1); else nbg[11]=*ptr1;
            if (i>0 && k<(int)depth-1) nbg[12]=get(data,width,height,depth,i-1,j,k+1); else nbg[12]=*ptr1;
            if (i<(int)width-1 && k<(int)depth-1) nbg[13]=get(data,width,height,depth,i+1,j,k+1); else nbg[13]=*ptr1;

            val=(*ptr1)+nbg[0]+nbg[4]+nbg[10]; if (val/4<mindata) mindata=val/4; else if ((val+3)/4>maxdata) maxdata=(val+3)/4;
            val=(*ptr1)+nbg[1]+nbg[4]+nbg[11]; if (val/4<mindata) mindata=val/4; else if ((val+3)/4>maxdata) maxdata=(val+3)/4;
            val=(*ptr1)+nbg[0]+nbg[5]+nbg[12]; if (val/4<mindata) mindata=val/4; else if ((val+3)/4>maxdata) maxdata=(val+3)/4;
            val=(*ptr1)+nbg[1]+nbg[5]+nbg[13]; if (val/4<mindata) mindata=val/4; else if ((val+3)/4>maxdata) maxdata=(val+3)/4;

            // yz-plane:

            if (j>0 && k>0) nbg[14]=get(data,width,height,depth,i,j-1,k-1); else nbg[14]=*ptr1;
            if (j<(int)height-1 && k>0) nbg[15]=get(data,width,height,depth,i,j+1,k-1); else nbg[15]=*ptr1;
            if (j>0 && k<(int)depth-1) nbg[16]=get(data,width,height,depth,i,j-1,k+1); else nbg[16]=*ptr1;
            if (j<(int)height-1 && k<(int)depth-1) nbg[17]=get(data,width,height,depth,i,j+1,k+1); else nbg[17]=*ptr1;

            val=(*ptr1)+nbg[2]+nbg[4]+nbg[14]; if (val/4<mindata) mindata=val/4; else if ((val+3)/4>maxdata) maxdata=(val+3)/4;
            val=(*ptr1)+nbg[3]+nbg[4]+nbg[15]; if (val/4<mindata) mindata=val/4; else if ((val+3)/4>maxdata) maxdata=(val+3)/4;
            val=(*ptr1)+nbg[2]+nbg[5]+nbg[16]; if (val/4<mindata) mindata=val/4; else if ((val+3)/4>maxdata) maxdata=(val+3)/4;
            val=(*ptr1)+nbg[3]+nbg[5]+nbg[17]; if (val/4<mindata) mindata=val/4; else if ((val+3)/4>maxdata) maxdata=(val+3)/4;

            // bottom xy-plane:

            if (i>0 && j>0 && k>0) nbg[18]=get(data,width,height,depth,i-1,j-1,k-1); else nbg[18]=*ptr1;
            if (i<(int)width-1 && j>0 && k>0) nbg[19]=get(data,width,height,depth,i+1,j-1,k-1); else nbg[19]=*ptr1;
            if (i>0 && j<(int)height-1 && k>0) nbg[20]=get(data,width,height,depth,i-1,j+1,k-1); else nbg[20]=*ptr1;
            if (i<(int)width-1 && j<(int)height-1 && k>0) nbg[21]=get(data,width,height,depth,i+1,j+1,k-1); else nbg[21]=*ptr1;

            val=(*ptr1)+nbg[0]+nbg[2]+nbg[6]+nbg[4]+nbg[10]+nbg[14]+nbg[18]; if (val/8<mindata) mindata=val/8; else if ((val+7)/8>maxdata) maxdata=(val+7)/8;
            val=(*ptr1)+nbg[1]+nbg[2]+nbg[7]+nbg[4]+nbg[11]+nbg[14]+nbg[19]; if (val/8<mindata) mindata=val/8; else if ((val+7)/8>maxdata) maxdata=(val+7)/8;
            val=(*ptr1)+nbg[0]+nbg[3]+nbg[8]+nbg[4]+nbg[10]+nbg[15]+nbg[20]; if (val/8<mindata) mindata=val/8; else if ((val+7)/8>maxdata) maxdata=(val+7)/8;
            val=(*ptr1)+nbg[1]+nbg[3]+nbg[9]+nbg[4]+nbg[11]+nbg[15]+nbg[21]; if (val/8<mindata) mindata=val/8; else if ((val+7)/8>maxdata) maxdata=(val+7)/8;

            // top xy-plane:

            if (i>0 && j>0 && k<(int)depth-1) nbg[22]=get(data,width,height,depth,i-1,j-1,k+1); else nbg[22]=*ptr1;
            if (i<(int)width-1 && j>0 && k<(int)depth-1) nbg[23]=get(data,width,height,depth,i+1,j-1,k+1); else nbg[23]=*ptr1;
            if (i>0 && j<(int)height-1 && k<(int)depth-1) nbg[24]=get(data,width,height,depth,i-1,j+1,k+1); else nbg[24]=*ptr1;
            if (i<(int)width-1 && j<(int)height-1 && k<(int)depth-1) nbg[25]=get(data,width,height,depth,i+1,j+1,k+1); else nbg[25]=*ptr1;

            val=(*ptr1)+nbg[0]+nbg[2]+nbg[6]+nbg[5]+nbg[12]+nbg[16]+nbg[22]; if (val/8<mindata) mindata=val/8; else if ((val+7)/8>maxdata) maxdata=(val+7)/8;
            val=(*ptr1)+nbg[1]+nbg[2]+nbg[7]+nbg[5]+nbg[13]+nbg[16]+nbg[23]; if (val/8<mindata) mindata=val/8; else if ((val+7)/8>maxdata) maxdata=(val+7)/8;
            val=(*ptr1)+nbg[0]+nbg[3]+nbg[8]+nbg[5]+nbg[12]+nbg[17]+nbg[24]; if (val/8<mindata) mindata=val/8; else if ((val+7)/8>maxdata) maxdata=(val+7)/8;
            val=(*ptr1)+nbg[1]+nbg[3]+nbg[9]+nbg[5]+nbg[13]+nbg[17]+nbg[25]; if (val/8<mindata) mindata=val/8; else if ((val+7)/8>maxdata) maxdata=(val+7)/8;

            // check for transparency
            if (TFUNC->zot(mindata/255.0f,maxdata/255.0f,(*ptr2)/255.0f,(*ptr2)/255.0f)) *ptr2=255;
            else *ptr2/=2;
            }

   TFUNC->set_mode(oldmode);
   }

// set gradient to maximum where opacity is zero
void mipmap::useop(unsigned char *data,unsigned char *grad,
                   unsigned int width,unsigned int height,unsigned int depth)
   {
   int i,j,k;

   unsigned char *ptr1,*ptr2;

   int mindata,maxdata;

   int val,nbg[26];

   int oldmode;

   oldmode=TFUNC->get_mode();

   if (!TFUNC->get_imode()) TFUNC->set_mode(0);

   TFUNC->premin();

   for (ptr1=data,ptr2=grad,k=0; k<(int)depth; k++)
      for (j=0; j<(int)height; j++)
         for (i=0; i<(int)width; i++,ptr1++,ptr2++)
            {
            // central voxel
            mindata=maxdata=*ptr1;

            // x-axis:

            if (i>0) nbg[0]=get(data,width,height,depth,i-1,j,k); else nbg[0]=*ptr1;
            if (i<(int)width-1) nbg[1]=get(data,width,height,depth,i+1,j,k); else nbg[1]=*ptr1;

            val=(*ptr1)+nbg[0]; if (val/2<mindata) mindata=val/2; else if ((val+1)/2>maxdata) maxdata=(val+1)/2;
            val=(*ptr1)+nbg[1]; if (val/2<mindata) mindata=val/2; else if ((val+1)/2>maxdata) maxdata=(val+1)/2;

            // y-axis:

            if (j>0) nbg[2]=get(data,width,height,depth,i,j-1,k); else nbg[2]=*ptr1;
            if (j<(int)height-1) nbg[3]=get(data,width,height,depth,i,j+1,k); else nbg[3]=*ptr1;

            val=(*ptr1)+nbg[2]; if (val/2<mindata) mindata=val/2; else if ((val+1)/2>maxdata) maxdata=(val+1)/2;
            val=(*ptr1)+nbg[3]; if (val/2<mindata) mindata=val/2; else if ((val+1)/2>maxdata) maxdata=(val+1)/2;

            // z-axis:

            if (k>0) nbg[4]=get(data,width,height,depth,i,j,k-1); else nbg[4]=*ptr1;
            if (k<(int)depth-1) nbg[5]=get(data,width,height,depth,i,j,k+1); else nbg[5]=*ptr1;

            val=(*ptr1)+nbg[4]; if (val/2<mindata) mindata=val/2; else if ((val+1)/2>maxdata) maxdata=(val+1)/2;
            val=(*ptr1)+nbg[5]; if (val/2<mindata) mindata=val/2; else if ((val+1)/2>maxdata) maxdata=(val+1)/2;

            // xy-plane:

            if (i>0 && j>0) nbg[6]=get(data,width,height,depth,i-1,j-1,k); else nbg[6]=*ptr1;
            if (i<(int)width-1 && j>0) nbg[7]=get(data,width,height,depth,i+1,j-1,k); else nbg[7]=*ptr1;
            if (i>0 && j<(int)height-1) nbg[8]=get(data,width,height,depth,i-1,j+1,k); else nbg[8]=*ptr1;
            if (i<(int)width-1 && j<(int)height-1) nbg[9]=get(data,width,height,depth,i+1,j+1,k); else nbg[9]=*ptr1;

            val=(*ptr1)+nbg[0]+nbg[2]+nbg[6]; if (val/4<mindata) mindata=val/4; else if ((val+3)/4>maxdata) maxdata=(val+3)/4;
            val=(*ptr1)+nbg[1]+nbg[2]+nbg[7]; if (val/4<mindata) mindata=val/4; else if ((val+3)/4>maxdata) maxdata=(val+3)/4;
            val=(*ptr1)+nbg[0]+nbg[3]+nbg[8]; if (val/4<mindata) mindata=val/4; else if ((val+3)/4>maxdata) maxdata=(val+3)/4;
            val=(*ptr1)+nbg[1]+nbg[3]+nbg[9]; if (val/4<mindata) mindata=val/4; else if ((val+3)/4>maxdata) maxdata=(val+3)/4;

            // xz-plane:

            if (i>0 && k>0) nbg[10]=get(data,width,height,depth,i-1,j,k-1); else nbg[10]=*ptr1;
            if (i<(int)width-1 && k>0) nbg[11]=get(data,width,height,depth,i+1,j,k-1); else nbg[11]=*ptr1;
            if (i>0 && k<(int)depth-1) nbg[12]=get(data,width,height,depth,i-1,j,k+1); else nbg[12]=*ptr1;
            if (i<(int)width-1 && k<(int)depth-1) nbg[13]=get(data,width,height,depth,i+1,j,k+1); else nbg[13]=*ptr1;

            val=(*ptr1)+nbg[0]+nbg[4]+nbg[10]; if (val/4<mindata) mindata=val/4; else if ((val+3)/4>maxdata) maxdata=(val+3)/4;
            val=(*ptr1)+nbg[1]+nbg[4]+nbg[11]; if (val/4<mindata) mindata=val/4; else if ((val+3)/4>maxdata) maxdata=(val+3)/4;
            val=(*ptr1)+nbg[0]+nbg[5]+nbg[12]; if (val/4<mindata) mindata=val/4; else if ((val+3)/4>maxdata) maxdata=(val+3)/4;
            val=(*ptr1)+nbg[1]+nbg[5]+nbg[13]; if (val/4<mindata) mindata=val/4; else if ((val+3)/4>maxdata) maxdata=(val+3)/4;

            // yz-plane:

            if (j>0 && k>0) nbg[14]=get(data,width,height,depth,i,j-1,k-1); else nbg[14]=*ptr1;
            if (j<(int)height-1 && k>0) nbg[15]=get(data,width,height,depth,i,j+1,k-1); else nbg[15]=*ptr1;
            if (j>0 && k<(int)depth-1) nbg[16]=get(data,width,height,depth,i,j-1,k+1); else nbg[16]=*ptr1;
            if (j<(int)height-1 && k<(int)depth-1) nbg[17]=get(data,width,height,depth,i,j+1,k+1); else nbg[17]=*ptr1;

            val=(*ptr1)+nbg[2]+nbg[4]+nbg[14]; if (val/4<mindata) mindata=val/4; else if ((val+3)/4>maxdata) maxdata=(val+3)/4;
            val=(*ptr1)+nbg[3]+nbg[4]+nbg[15]; if (val/4<mindata) mindata=val/4; else if ((val+3)/4>maxdata) maxdata=(val+3)/4;
            val=(*ptr1)+nbg[2]+nbg[5]+nbg[16]; if (val/4<mindata) mindata=val/4; else if ((val+3)/4>maxdata) maxdata=(val+3)/4;
            val=(*ptr1)+nbg[3]+nbg[5]+nbg[17]; if (val/4<mindata) mindata=val/4; else if ((val+3)/4>maxdata) maxdata=(val+3)/4;

            // bottom xy-plane:

            if (i>0 && j>0 && k>0) nbg[18]=get(data,width,height,depth,i-1,j-1,k-1); else nbg[18]=*ptr1;
            if (i<(int)width-1 && j>0 && k>0) nbg[19]=get(data,width,height,depth,i+1,j-1,k-1); else nbg[19]=*ptr1;
            if (i>0 && j<(int)height-1 && k>0) nbg[20]=get(data,width,height,depth,i-1,j+1,k-1); else nbg[20]=*ptr1;
            if (i<(int)width-1 && j<(int)height-1 && k>0) nbg[21]=get(data,width,height,depth,i+1,j+1,k-1); else nbg[21]=*ptr1;

            val=(*ptr1)+nbg[0]+nbg[2]+nbg[6]+nbg[4]+nbg[10]+nbg[14]+nbg[18]; if (val/8<mindata) mindata=val/8; else if ((val+7)/8>maxdata) maxdata=(val+7)/8;
            val=(*ptr1)+nbg[1]+nbg[2]+nbg[7]+nbg[4]+nbg[11]+nbg[14]+nbg[19]; if (val/8<mindata) mindata=val/8; else if ((val+7)/8>maxdata) maxdata=(val+7)/8;
            val=(*ptr1)+nbg[0]+nbg[3]+nbg[8]+nbg[4]+nbg[10]+nbg[15]+nbg[20]; if (val/8<mindata) mindata=val/8; else if ((val+7)/8>maxdata) maxdata=(val+7)/8;
            val=(*ptr1)+nbg[1]+nbg[3]+nbg[9]+nbg[4]+nbg[11]+nbg[15]+nbg[21]; if (val/8<mindata) mindata=val/8; else if ((val+7)/8>maxdata) maxdata=(val+7)/8;

            // top xy-plane:

            if (i>0 && j>0 && k<(int)depth-1) nbg[22]=get(data,width,height,depth,i-1,j-1,k+1); else nbg[22]=*ptr1;
            if (i<(int)width-1 && j>0 && k<(int)depth-1) nbg[23]=get(data,width,height,depth,i+1,j-1,k+1); else nbg[23]=*ptr1;
            if (i>0 && j<(int)height-1 && k<(int)depth-1) nbg[24]=get(data,width,height,depth,i-1,j+1,k+1); else nbg[24]=*ptr1;
            if (i<(int)width-1 && j<(int)height-1 && k<(int)depth-1) nbg[25]=get(data,width,height,depth,i+1,j+1,k+1); else nbg[25]=*ptr1;

            val=(*ptr1)+nbg[0]+nbg[2]+nbg[6]+nbg[5]+nbg[12]+nbg[16]+nbg[22]; if (val/8<mindata) mindata=val/8; else if ((val+7)/8>maxdata) maxdata=(val+7)/8;
            val=(*ptr1)+nbg[1]+nbg[2]+nbg[7]+nbg[5]+nbg[13]+nbg[16]+nbg[23]; if (val/8<mindata) mindata=val/8; else if ((val+7)/8>maxdata) maxdata=(val+7)/8;
            val=(*ptr1)+nbg[0]+nbg[3]+nbg[8]+nbg[5]+nbg[12]+nbg[17]+nbg[24]; if (val/8<mindata) mindata=val/8; else if ((val+7)/8>maxdata) maxdata=(val+7)/8;
            val=(*ptr1)+nbg[1]+nbg[3]+nbg[9]+nbg[5]+nbg[13]+nbg[17]+nbg[25]; if (val/8<mindata) mindata=val/8; else if ((val+7)/8>maxdata) maxdata=(val+7)/8;

            // check for transparency
            if (TFUNC->mot(mindata/255.0f,maxdata/255.0f,(*ptr2)/255.0f,(*ptr2)/255.0f)) *ptr2=255;
            else *ptr2/=2;
            }

   TFUNC->set_mode(oldmode);
   }

// remove bubbles
void mipmap::remove(unsigned char *grad,
                    unsigned int width,unsigned int height,unsigned int depth)
   {
   int i,j,k;

   unsigned char *ptr;

   int nbg,cnt;

   for (ptr=grad,k=0; k<(int)depth; k++)
      {
      cache(grad,width,height,depth,k-1,2);

      for (j=0; j<(int)height; j++)
         for (i=0; i<(int)width; i++,ptr++)
            if (*ptr==255)
               {
               nbg=cnt=0;

               if (i>0) {if (get(grad,width,height,depth,i-1,j,k)==255) nbg++; cnt++;}
               if (i<(int)width-1) {if (get(grad,width,height,depth,i+1,j,k)==255) nbg++; cnt++;}

               if (j>0) {if (get(grad,width,height,depth,i,j-1,k)==255) nbg++; cnt++;}
               if (j<(int)height-1) {if (get(grad,width,height,depth,i,j+1,k)==255) nbg++; cnt++;}

               if (k>0) {if (get(grad,width,height,depth,i,j,k-1)==255) nbg++; cnt++;}
               if (k<(int)depth-1) {if (get(grad,width,height,depth,i,j,k+1)==255) nbg++; cnt++;}

               if (nbg==cnt) *ptr=0;
               }
      }

   cache();
   }

// tangle material
void mipmap::tangle(unsigned char *grad,
                    unsigned int width,unsigned int height,unsigned int depth)
   {
   int i,j,k;

   unsigned char *ptr;

   int cnt;

   for (ptr=grad,k=0; k<(int)depth; k++)
      {
      cache(grad,width,height,depth,k-1,2);

      for (j=0; j<(int)height; j++)
         for (i=0; i<(int)width; i++,ptr++)
            if (*ptr==255)
               {
               cnt=0;

               // x-axis
               if (i>0) if (get(grad,width,height,depth,i-1,j,k)<255) cnt++;
               if (i<(int)width-1) if (get(grad,width,height,depth,i+1,j,k)<255) cnt++;

               // y-axis
               if (j>0) if (get(grad,width,height,depth,i,j-1,k)<255) cnt++;
               if (j<(int)height-1) if (get(grad,width,height,depth,i,j+1,k)<255) cnt++;

               // z-axis
               if (k>0) if (get(grad,width,height,depth,i,j,k-1)<255) cnt++;
               if (k<(int)depth-1) if (get(grad,width,height,depth,i,j,k+1)<255) cnt++;

               // xy-plane
               if (i>0 && j>0) if (get(grad,width,height,depth,i-1,j-1,k)<255) cnt++;
               if (i<(int)width-1 && j>0) if (get(grad,width,height,depth,i+1,j-1,k)<255) cnt++;
               if (i>0 && j<(int)height-1) if (get(grad,width,height,depth,i-1,j+1,k)<255) cnt++;
               if (i<(int)width-1 && j<(int)height-1) if (get(grad,width,height,depth,i+1,j+1,k)<255) cnt++;

               // xz-plane
               if (i>0 && k>0) if (get(grad,width,height,depth,i-1,j,k-1)<255) cnt++;
               if (i<(int)width-1 && k>0) if (get(grad,width,height,depth,i+1,j,k-1)<255) cnt++;
               if (i>0 && k<(int)depth-1) if (get(grad,width,height,depth,i-1,j,k+1)<255) cnt++;
               if (i<(int)width-1 && k<(int)depth-1) if (get(grad,width,height,depth,i+1,j,k+1)<255) cnt++;

               // yz-plane
               if (j>0 && k>0) if (get(grad,width,height,depth,i,j-1,k-1)<255) cnt++;
               if (j<(int)height-1 && k>0) if (get(grad,width,height,depth,i,j+1,k-1)<255) cnt++;
               if (j>0 && k<(int)depth-1) if (get(grad,width,height,depth,i,j-1,k+1)<255) cnt++;
               if (j<(int)height-1 && k<(int)depth-1) if (get(grad,width,height,depth,i,j+1,k+1)<255) cnt++;

               // bottom xy-plane
               if (i>0 && j>0 && k>0) if (get(grad,width,height,depth,i-1,j-1,k-1)<255) cnt++;
               if (i<(int)width-1 && j>0 && k>0) if (get(grad,width,height,depth,i+1,j-1,k-1)<255) cnt++;
               if (i>0 && j<(int)height-1 && k>0) if (get(grad,width,height,depth,i-1,j+1,k-1)<255) cnt++;
               if (i<(int)width-1 && j<(int)height-1 && k>0) if (get(grad,width,height,depth,i+1,j+1,k-1)<255) cnt++;

               // top xy-plane
               if (i>0 && j>0 && k<(int)depth-1) if (get(grad,width,height,depth,i-1,j-1,k+1)<255) cnt++;
               if (i<(int)width-1 && j>0 && k<(int)depth-1) if (get(grad,width,height,depth,i+1,j-1,k+1)<255) cnt++;
               if (i>0 && j<(int)height-1 && k<(int)depth-1) if (get(grad,width,height,depth,i-1,j+1,k+1)<255) cnt++;
               if (i<(int)width-1 && j<(int)height-1 && k<(int)depth-1) if (get(grad,width,height,depth,i+1,j+1,k+1)<255) cnt++;

               if (cnt>0) *ptr=0;
               }
      }

   cache();
   }

// grow material
unsigned int mipmap::grow(unsigned char *grad,
                          unsigned int width,unsigned int height,unsigned int depth)
   {
   int i,j,k;
   int v,c;

   unsigned char *ptr;

   int cnt,maxcnt;

   int val,nbg[26];

   unsigned int found;

   found=0;

   for (ptr=grad,k=0; k<(int)depth; k++)
      {
      cache(grad,width,height,depth,k-1,2);

      for (j=0; j<(int)height; j++)
         for (i=0; i<(int)width; i++,ptr++)
            if (*ptr==0)
               {
               cnt=0;

               // x-axis
               if (i>0) {val=get(grad,width,height,depth,i-1,j,k); if (val>0) cnt++; nbg[0]=val;} else nbg[0]=0;
               if (i<(int)width-1) {val=get(grad,width,height,depth,i+1,j,k); if (val>0) cnt++; nbg[1]=val;} else nbg[1]=0;

               // y-axis
               if (j>0) {val=get(grad,width,height,depth,i,j-1,k); if (val>0) cnt++; nbg[2]=val;} else nbg[2]=0;
               if (j<(int)height-1) {val=get(grad,width,height,depth,i,j+1,k); if (val>0) cnt++; nbg[3]=val;} else nbg[3]=0;

               // z-axis
               if (k>0) {val=get(grad,width,height,depth,i,j,k-1); if (val>0) cnt++; nbg[4]=val;} else nbg[4]=0;
               if (k<(int)depth-1) {val=get(grad,width,height,depth,i,j,k+1); if (val>0) cnt++; nbg[5]=val;} else nbg[5]=0;

               // xy-plane
               if (i>0 && j>0) {val=get(grad,width,height,depth,i-1,j-1,k); if (val>0) cnt++; nbg[6]=val;} else nbg[6]=0;
               if (i<(int)width-1 && j>0) {val=get(grad,width,height,depth,i+1,j-1,k); if (val>0) cnt++; nbg[7]=val;} else nbg[7]=0;
               if (i>0 && j<(int)height-1) {val=get(grad,width,height,depth,i-1,j+1,k); if (val>0) cnt++; nbg[8]=val;} else nbg[8]=0;
               if (i<(int)width-1 && j<(int)height-1) {val=get(grad,width,height,depth,i+1,j+1,k); if (val>0) cnt++; nbg[9]=val;} else nbg[9]=0;

               // xz-plane
               if (i>0 && k>0) {val=get(grad,width,height,depth,i-1,j,k-1); if (val>0) cnt++; nbg[10]=val;} else nbg[10]=0;
               if (i<(int)width-1 && k>0) {val=get(grad,width,height,depth,i+1,j,k-1); if (val>0) cnt++; nbg[11]=val;} else nbg[11]=0;
               if (i>0 && k<(int)depth-1) {val=get(grad,width,height,depth,i-1,j,k+1); if (val>0) cnt++; nbg[12]=val;} else nbg[12]=0;
               if (i<(int)width-1 && k<(int)depth-1) {val=get(grad,width,height,depth,i+1,j,k+1); if (val>0) cnt++; nbg[13]=val;} else nbg[13]=0;

               // yz-plane
               if (j>0 && k>0) {val=get(grad,width,height,depth,i,j-1,k-1); if (val>0) cnt++; nbg[14]=val;} else nbg[14]=0;
               if (j<(int)height-1 && k>0) {val=get(grad,width,height,depth,i,j+1,k-1); if (val>0) cnt++; nbg[15]=val;} else nbg[15]=0;
               if (j>0 && k<(int)depth-1) {val=get(grad,width,height,depth,i,j-1,k+1); if (val>0) cnt++; nbg[16]=val;} else nbg[16]=0;
               if (j<(int)height-1 && k<(int)depth-1) {val=get(grad,width,height,depth,i,j+1,k+1); if (val>0) cnt++; nbg[17]=val;} else nbg[17]=0;

               // bottom xy-plane
               if (i>0 && j>0 && k>0) {val=get(grad,width,height,depth,i-1,j-1,k-1); if (val>0) cnt++; nbg[18]=val;} else nbg[18]=0;
               if (i<(int)width-1 && j>0 && k>0) {val=get(grad,width,height,depth,i+1,j-1,k-1); if (val>0) cnt++; nbg[19]=val;} else nbg[19]=0;
               if (i>0 && j<(int)height-1 && k>0) {val=get(grad,width,height,depth,i-1,j+1,k-1); if (val>0) cnt++; nbg[20]=val;} else nbg[20]=0;
               if (i<(int)width-1 && j<(int)height-1 && k>0) {val=get(grad,width,height,depth,i+1,j+1,k-1); if (val>0) cnt++; nbg[21]=val;} else nbg[21]=0;

               // top xy-plane
               if (i>0 && j>0 && k<(int)depth-1) {val=get(grad,width,height,depth,i-1,j-1,k+1); if (val>0) cnt++; nbg[22]=val;} else nbg[22]=0;
               if (i<(int)width-1 && j>0 && k<(int)depth-1) {val=get(grad,width,height,depth,i+1,j-1,k+1); if (val>0) cnt++; nbg[23]=val;} else nbg[23]=0;
               if (i>0 && j<(int)height-1 && k<(int)depth-1) {val=get(grad,width,height,depth,i-1,j+1,k+1); if (val>0) cnt++; nbg[24]=val;} else nbg[24]=0;
               if (i<(int)width-1 && j<(int)height-1 && k<(int)depth-1) {val=get(grad,width,height,depth,i+1,j+1,k+1); if (val>0) cnt++; nbg[25]=val;} else nbg[25]=0;

               if (cnt>0)
                  {
                  val=0;
                  maxcnt=0;

                  for (v=0; v<26; v++)
                     if (nbg[v]>0)
                        {
                        cnt=1;

                        for (c=v+1; c<26; c++)
                           if (nbg[c]==nbg[v])
                              {
                              cnt++;
                              nbg[c]=0;
                              }

                        if (cnt>maxcnt)
                           {
                           maxcnt=cnt;
                           val=nbg[v];
                           }
                        }

                  *ptr=val;
                  found++;
                  }
               }
      }

   cache();

   return(found);
   }

// floodfill a segment of the volume based on scalar value
unsigned int mipmap::floodfill(const unsigned char *data,unsigned char *mark,
                               const unsigned int width,const unsigned int height,const unsigned int depth,
                               const unsigned int x,const unsigned int y,const unsigned int z,
                               const int value,const int maxdev,
                               const int token)
   {
   unsigned int i;
   unsigned int cnt;

   int xs,ys,zs;
   int *qx,*qy,*qz;

   cnt=0;

   QUEUECNT=1;
   QUEUESTART=0;
   QUEUEEND=1;

   QUEUEX[QUEUESTART]=x;
   QUEUEY[QUEUESTART]=y;
   QUEUEZ[QUEUESTART]=z;

   while (QUEUECNT>0)
      {
      xs=QUEUEX[QUEUESTART];
      ys=QUEUEY[QUEUESTART];
      zs=QUEUEZ[QUEUESTART];

      QUEUESTART++;
      QUEUESTART%=QUEUEMAX;
      QUEUECNT--;

      if (get(mark,width,height,depth,xs,ys,zs)==token) continue;
      if (abs(get(data,width,height,depth,xs,ys,zs)-value)>maxdev) continue;

      set(mark,width,height,depth,xs,ys,zs,token);
      cnt++;

      if (QUEUECNT+6>QUEUEMAX)
         {
         qx=new int[QUEUEMAX+QUEUEINC];
         qy=new int[QUEUEMAX+QUEUEINC];
         qz=new int[QUEUEMAX+QUEUEINC];

         for (i=0; i<QUEUECNT; i++)
            {
            qx[i]=QUEUEX[QUEUESTART];
            qy[i]=QUEUEY[QUEUESTART];
            qz[i]=QUEUEZ[QUEUESTART];

            QUEUESTART++;
            QUEUESTART%=QUEUEMAX;
            }

         delete QUEUEX;
         delete QUEUEY;
         delete QUEUEZ;

         QUEUEX=qx;
         QUEUEY=qy;
         QUEUEZ=qz;

         QUEUEMAX+=QUEUEINC;

         QUEUESTART=0;
         QUEUEEND=QUEUECNT;
         }

      if (xs>0)
         {
         QUEUEX[QUEUEEND]=xs-1;
         QUEUEY[QUEUEEND]=ys;
         QUEUEZ[QUEUEEND]=zs;

         QUEUEEND++;
         QUEUEEND%=QUEUEMAX;
         QUEUECNT++;
         }

      if (xs<(int)width-1)
         {
         QUEUEX[QUEUEEND]=xs+1;
         QUEUEY[QUEUEEND]=ys;
         QUEUEZ[QUEUEEND]=zs;

         QUEUEEND++;
         QUEUEEND%=QUEUEMAX;
         QUEUECNT++;
         }

      if (ys>0)
         {
         QUEUEX[QUEUEEND]=xs;
         QUEUEY[QUEUEEND]=ys-1;
         QUEUEZ[QUEUEEND]=zs;

         QUEUEEND++;
         QUEUEEND%=QUEUEMAX;
         QUEUECNT++;
         }

      if (ys<(int)height-1)
         {
         QUEUEX[QUEUEEND]=xs;
         QUEUEY[QUEUEEND]=ys+1;
         QUEUEZ[QUEUEEND]=zs;

         QUEUEEND++;
         QUEUEEND%=QUEUEMAX;
         QUEUECNT++;
         }

      if (zs>0)
         {
         QUEUEX[QUEUEEND]=xs;
         QUEUEY[QUEUEEND]=ys;
         QUEUEZ[QUEUEEND]=zs-1;

         QUEUEEND++;
         QUEUEEND%=QUEUEMAX;
         QUEUECNT++;
         }

      if (zs<(int)depth-1)
         {
         QUEUEX[QUEUEEND]=xs;
         QUEUEY[QUEUEEND]=ys;
         QUEUEZ[QUEUEEND]=zs+1;

         QUEUEEND++;
         QUEUEEND%=QUEUEMAX;
         QUEUECNT++;
         }
      }

   return(cnt);
   }

// floodfill a segment of the volume counting space
float mipmap::countfill(const unsigned char *data,unsigned char *mark,
                        const unsigned int width,const unsigned int height,const unsigned int depth,
                        const unsigned int x,const unsigned int y,const unsigned int z,
                        const int value,const int maxdev,
                        const int token)
   {
   unsigned int i;
   float cnt;

   int xs,ys,zs;
   int *qx,*qy,*qz;

   cnt=0.0f;

   QUEUECNT=1;
   QUEUESTART=0;
   QUEUEEND=1;

   QUEUEX[QUEUESTART]=x;
   QUEUEY[QUEUESTART]=y;
   QUEUEZ[QUEUESTART]=z;

   while (QUEUECNT>0)
      {
      xs=QUEUEX[QUEUESTART];
      ys=QUEUEY[QUEUESTART];
      zs=QUEUEZ[QUEUESTART];

      QUEUESTART++;
      QUEUESTART%=QUEUEMAX;
      QUEUECNT--;

      if (get(mark,width,height,depth,xs,ys,zs)==token) continue;
      if (abs(get(data,width,height,depth,xs,ys,zs)-value)>maxdev) continue;

      set(mark,width,height,depth,xs,ys,zs,token);
      cnt+=1.0f-get(data,width,height,depth,xs,ys,zs)/255.0f;

      if (QUEUECNT+6>QUEUEMAX)
         {
         qx=new int[QUEUEMAX+QUEUEINC];
         qy=new int[QUEUEMAX+QUEUEINC];
         qz=new int[QUEUEMAX+QUEUEINC];

         for (i=0; i<QUEUECNT; i++)
            {
            qx[i]=QUEUEX[QUEUESTART];
            qy[i]=QUEUEY[QUEUESTART];
            qz[i]=QUEUEZ[QUEUESTART];

            QUEUESTART++;
            QUEUESTART%=QUEUEMAX;
            }

         delete QUEUEX;
         delete QUEUEY;
         delete QUEUEZ;

         QUEUEX=qx;
         QUEUEY=qy;
         QUEUEZ=qz;

         QUEUEMAX+=QUEUEINC;

         QUEUESTART=0;
         QUEUEEND=QUEUECNT;
         }

      if (xs>0)
         {
         QUEUEX[QUEUEEND]=xs-1;
         QUEUEY[QUEUEEND]=ys;
         QUEUEZ[QUEUEEND]=zs;

         QUEUEEND++;
         QUEUEEND%=QUEUEMAX;
         QUEUECNT++;
         }

      if (xs<(int)width-1)
         {
         QUEUEX[QUEUEEND]=xs+1;
         QUEUEY[QUEUEEND]=ys;
         QUEUEZ[QUEUEEND]=zs;

         QUEUEEND++;
         QUEUEEND%=QUEUEMAX;
         QUEUECNT++;
         }

      if (ys>0)
         {
         QUEUEX[QUEUEEND]=xs;
         QUEUEY[QUEUEEND]=ys-1;
         QUEUEZ[QUEUEEND]=zs;

         QUEUEEND++;
         QUEUEEND%=QUEUEMAX;
         QUEUECNT++;
         }

      if (ys<(int)height-1)
         {
         QUEUEX[QUEUEEND]=xs;
         QUEUEY[QUEUEEND]=ys+1;
         QUEUEZ[QUEUEEND]=zs;

         QUEUEEND++;
         QUEUEEND%=QUEUEMAX;
         QUEUECNT++;
         }

      if (zs>0)
         {
         QUEUEX[QUEUEEND]=xs;
         QUEUEY[QUEUEEND]=ys;
         QUEUEZ[QUEUEEND]=zs-1;

         QUEUEEND++;
         QUEUEEND%=QUEUEMAX;
         QUEUECNT++;
         }

      if (zs<(int)depth-1)
         {
         QUEUEX[QUEUEEND]=xs;
         QUEUEY[QUEUEEND]=ys;
         QUEUEZ[QUEUEEND]=zs+1;

         QUEUEEND++;
         QUEUEEND%=QUEUEMAX;
         QUEUECNT++;
         }
      }

   return(cnt);
   }

// floodfill a segment of the volume based on gradient magnitude
unsigned int mipmap::gradfill(const unsigned char *grad,unsigned char *mark,
                              const unsigned int width,const unsigned int height,const unsigned int depth,
                              const unsigned int x,const unsigned int y,const unsigned int z,
                              const int token,const int maxgrad)
   {
   unsigned int i;
   unsigned int cnt;

   int xs,ys,zs;
   int *qx,*qy,*qz;

   cnt=0;

   QUEUECNT=1;
   QUEUESTART=0;
   QUEUEEND=1;

   QUEUEX[QUEUESTART]=x;
   QUEUEY[QUEUESTART]=y;
   QUEUEZ[QUEUESTART]=z;

   while (QUEUECNT>0)
      {
      xs=QUEUEX[QUEUESTART];
      ys=QUEUEY[QUEUESTART];
      zs=QUEUEZ[QUEUESTART];

      QUEUESTART++;
      QUEUESTART%=QUEUEMAX;
      QUEUECNT--;

      if (get(mark,width,height,depth,xs,ys,zs)!=0) continue;
      if (get(grad,width,height,depth,xs,ys,zs)>=maxgrad) continue;

      set(mark,width,height,depth,xs,ys,zs,token);
      cnt++;

      if (QUEUECNT+6>QUEUEMAX)
         {
         qx=new int[QUEUEMAX+QUEUEINC];
         qy=new int[QUEUEMAX+QUEUEINC];
         qz=new int[QUEUEMAX+QUEUEINC];

         for (i=0; i<QUEUECNT; i++)
            {
            qx[i]=QUEUEX[QUEUESTART];
            qy[i]=QUEUEY[QUEUESTART];
            qz[i]=QUEUEZ[QUEUESTART];

            QUEUESTART++;
            QUEUESTART%=QUEUEMAX;
            }

         delete QUEUEX;
         delete QUEUEY;
         delete QUEUEZ;

         QUEUEX=qx;
         QUEUEY=qy;
         QUEUEZ=qz;

         QUEUEMAX+=QUEUEINC;

         QUEUESTART=0;
         QUEUEEND=QUEUECNT;
         }

      if (xs>0)
         {
         QUEUEX[QUEUEEND]=xs-1;
         QUEUEY[QUEUEEND]=ys;
         QUEUEZ[QUEUEEND]=zs;

         QUEUEEND++;
         QUEUEEND%=QUEUEMAX;
         QUEUECNT++;
         }

      if (xs<(int)width-1)
         {
         QUEUEX[QUEUEEND]=xs+1;
         QUEUEY[QUEUEEND]=ys;
         QUEUEZ[QUEUEEND]=zs;

         QUEUEEND++;
         QUEUEEND%=QUEUEMAX;
         QUEUECNT++;
         }

      if (ys>0)
         {
         QUEUEX[QUEUEEND]=xs;
         QUEUEY[QUEUEEND]=ys-1;
         QUEUEZ[QUEUEEND]=zs;

         QUEUEEND++;
         QUEUEEND%=QUEUEMAX;
         QUEUECNT++;
         }

      if (ys<(int)height-1)
         {
         QUEUEX[QUEUEEND]=xs;
         QUEUEY[QUEUEEND]=ys+1;
         QUEUEZ[QUEUEEND]=zs;

         QUEUEEND++;
         QUEUEEND%=QUEUEMAX;
         QUEUECNT++;
         }

      if (zs>0)
         {
         QUEUEX[QUEUEEND]=xs;
         QUEUEY[QUEUEEND]=ys;
         QUEUEZ[QUEUEEND]=zs-1;

         QUEUEEND++;
         QUEUEEND%=QUEUEMAX;
         QUEUECNT++;
         }

      if (zs<(int)depth-1)
         {
         QUEUEX[QUEUEEND]=xs;
         QUEUEY[QUEUEEND]=ys;
         QUEUEZ[QUEUEEND]=zs+1;

         QUEUEEND++;
         QUEUEEND%=QUEUEMAX;
         QUEUECNT++;
         }
      }

   return(cnt);
   }

// classify the volume by the segment size
unsigned char *mipmap::sizify(unsigned char *data,
                              unsigned int width,unsigned int height,unsigned int depth,
                              float maxdev)
   {
   int i,j,k;

   unsigned char *data2,*ptr2;

   unsigned int size,maxsize;
   int maxd;

   if ((data2=(unsigned char *)malloc(width*height*depth))==NULL) ERRORMSG();

   for (ptr2=data2,k=0; k<(int)depth; k++)
      for (j=0; j<(int)height; j++)
         for (i=0; i<(int)width; i++) *ptr2++=0;

   maxsize=1;
   maxd=ftrc(255.0f*maxdev+0.5f);

   for (k=0; k<(int)depth; k++)
      for (j=0; j<(int)height; j++)
         for (i=0; i<(int)width; i++)
            if (get(data2,width,height,depth,i,j,k)==0)
               {
               size=floodfill(data,data2,
                              width,height,depth,
                              i,j,k,get(data,width,height,depth,i,j,k),
                              maxd,1);

               if (size>maxsize) maxsize=size;
               }

   for (ptr2=data2,k=0; k<(int)depth; k++)
      for (j=0; j<(int)height; j++)
         for (i=0; i<(int)width; i++) *ptr2++=0;

   for (k=0; k<(int)depth; k++)
      for (j=0; j<(int)height; j++)
         for (i=0; i<(int)width; i++)
            if (get(data2,width,height,depth,i,j,k)==0)
               {
               size=floodfill(data,data2,
                              width,height,depth,
                              i,j,k,get(data,width,height,depth,i,j,k),
                              maxd,1);

               size=ftrc(255.0f*(1.0f-fpow((float)size/maxsize,1.0f/3))+0.5f);
               if (size==0) size=1;

               floodfill(data,data2,
                         width,height,depth,
                         i,j,k,get(data,width,height,depth,i,j,k),
                         maxd,size);
               }

   return(data2);
   }

// classify the volume by gradient border
unsigned char *mipmap::classify(unsigned char *grad,
                                unsigned int width,unsigned int height,unsigned int depth,
                                float maxgrad,
                                unsigned int *classes)
   {
   const int stepping=71;

   int i,j,k;

   unsigned char *data2,*ptr;

   int token;
   int maxg;

   unsigned int cnt;

   if ((data2=(unsigned char *)malloc(width*height*depth))==NULL) ERRORMSG();

   for (ptr=data2,k=0; k<(int)depth; k++)
      for (j=0; j<(int)height; j++)
         for (i=0; i<(int)width; i++) *ptr++=0;

   token=128;
   maxg=ftrc(255.0f*maxgrad+0.5f);
   cnt=0;

   for (ptr=data2,k=0; k<(int)depth; k++)
      for (j=0; j<(int)height; j++)
         for (i=0; i<(int)width; i++)
            if (*ptr++==0)
               if (get(grad,width,height,depth,i,j,k)<maxg)
                  {
                  if (token==0) token=(token+stepping)%256;

                  gradfill(grad,data2,
                           width,height,depth,
                           i,j,k,token,maxg);

                  token=(token+stepping)%256;
                  cnt++;
                  }

   if (classes!=NULL) *classes=cnt;

   return(data2);
   }

// zero space
void mipmap::zero(unsigned char *data,unsigned char *grad,
                  unsigned int width,unsigned int height,unsigned int depth,
                  float maxdev)
   {
   int i,j,k;

   unsigned char *data2,*ptr;

   int maxd;

   float cnt,maxcnt;
   unsigned int mi,mj,mk;

   if ((data2=(unsigned char *)malloc(width*height*depth))==NULL) ERRORMSG();

   for (ptr=data2,k=0; k<(int)depth; k++)
      for (j=0; j<(int)height; j++)
         for (i=0; i<(int)width; i++) *ptr++=0;

   maxd=ftrc(255.0f*maxdev+0.5f);

   maxcnt=0.0f;
   mi=mj=mk=0;

   for (ptr=data2,k=0; k<(int)depth; k++)
      for (j=0; j<(int)height; j++)
         for (i=0; i<(int)width; i++)
            if (*ptr++==0)
               {
               cnt=countfill(grad,data2,
                             width,height,depth,
                             i,j,k,get(grad,width,height,depth,i,j,k),maxd,1);

               if (cnt>maxcnt)
                  {
                  maxcnt=cnt;

                  mi=i;
                  mj=j;
                  mk=k;
                  }
               }

   free(data2);

   floodfill(grad,grad,
             width,height,depth,
             mi,mj,mk,get(grad,width,height,depth,mi,mj,mk),maxd,0);
   }

// parse command string
void mipmap::parsecommands(unsigned char *volume,
                           unsigned int width,unsigned int height,unsigned int depth,
                           char *commands)
   {
   char command;

   if (commands==NULL) return;

   while ((command=*commands++)!='\0')
      switch (command)
         {
         case ' ': // nop
            break;
         case 'b': // blur volume
            blur(volume,width,height,depth);
            break;
         }
   }

// parse gradient command string
void mipmap::parsegradcommands(unsigned char *volume,unsigned char *grad,
                               unsigned int width,unsigned int height,unsigned int depth,
                               char *commands)
   {
   const float maxdev=0.1f;
   const float maxgrad=0.5f;

   char command;

   unsigned char *volume2;

   if (commands==NULL) return;

   while ((command=*commands++)!='\0')
      switch (command)
         {
         case ' ': // nop
         case 'b':
            break;
         case 'd': // derive curvature
            volume2=gradmag(grad,width,height,depth,DSX,DSY,DSZ);
            memcpy(grad,volume2,width*height*depth);
            free(volume2);
            break;
         case 'v': // calculate variance
            volume2=variance(volume,width,height,depth);
            memcpy(grad,volume2,width*height*depth);
            free(volume2);
            break;
         case 'u': // use transfer function
            usetf(volume,grad,width,height,depth);
            break;
         case 'o': // use opacity
            useop(volume,grad,width,height,depth);
            break;
         case 't': // tangle material
            tangle(grad,width,height,depth);
            break;
         case 'r': // remove bubbles
            remove(grad,width,height,depth);
            break;
         case 's': // sizify volume
            volume2=sizify(volume,width,height,depth,maxdev);
            memcpy(grad,volume2,width*height*depth);
            free(volume2);
            break;
         case 'c': // classify volume
            volume2=classify(grad,width,height,depth,maxgrad);
            memcpy(grad,volume2,width*height*depth);
            free(volume2);
            break;
         case 'z': // zero space
            zero(volume,grad,width,height,depth,0.0f);
            break;
         case 'S': // sizify gradient
            volume2=sizify(grad,width,height,depth,0.0f);
            memcpy(grad,volume2,width*height*depth);
            free(volume2);
            break;
         case 'T': // grow material
            grow(grad,width,height,depth);
            break;
         case 'F': // fill border
            grow(grad,width,height,depth);
            break;
         case 'R': // fill space
            while (grow(grad,width,height,depth)>0);
            break;
         default: ERRORMSG();
         }
   }

// get interpolated scalar value from volume
unsigned char mipmap::getscalar(unsigned char *volume,
                                unsigned int width,unsigned int height,unsigned int depth,
                                float x,float y,float z)
   {
   int i,j,k;

   unsigned char *ptr1,*ptr2;

   x*=width-1;
   y*=height-1;
   z*=depth-1;

   i=ftrc(x);
   j=ftrc(y);
   k=ftrc(z);

   x-=i;
   y-=j;
   z-=k;

   if (i<0)
      {
      i=0;
      x=0.0f;
      }

   if (j<0)
      {
      j=0;
      y=0.0f;
      }

   if (k<0)
      {
      k=0;
      z=0.0f;
      }

   if (i>=(int)width-1)
      {
      i=width-2;
      x=1.0f;
      }

   if (j>=(int)height-1)
      {
      j=height-2;
      y=1.0f;
      }

   if (k>=(int)depth-1)
      {
      k=depth-2;
      z=1.0f;
      }

   ptr1=&volume[(unsigned int)i+((unsigned int)j+(unsigned int)k*height)*width];
   ptr2=ptr1+width*height;

   return(ftrc((1.0f-z)*((1.0f-y)*((1.0f-x)*ptr1[0]+x*ptr1[1])+
                         y*((1.0f-x)*ptr1[width]+x*ptr1[width+1]))+
               z*((1.0f-y)*((1.0f-x)*ptr2[0]+x*ptr2[1])+
                  y*((1.0f-x)*ptr2[width]+x*ptr2[width+1]))+0.5f));
   }

// get interpolated scalar value from volume
float mipmap::getscalar(float *volume,
                        unsigned int width,unsigned int height,unsigned int depth,
                        float x,float y,float z)
   {
   int i,j,k;

   float *ptr1,*ptr2;

   x*=width-1;
   y*=height-1;
   z*=depth-1;

   i=ftrc(x);
   j=ftrc(y);
   k=ftrc(z);

   x-=i;
   y-=j;
   z-=k;

   if (i<0)
      {
      i=0;
      x=0.0f;
      }

   if (j<0)
      {
      j=0;
      y=0.0f;
      }

   if (k<0)
      {
      k=0;
      z=0.0f;
      }

   if (i>=(int)width-1)
      {
      i=width-2;
      x=1.0f;
      }

   if (j>=(int)height-1)
      {
      j=height-2;
      y=1.0f;
      }

   if (k>=(int)depth-1)
      {
      k=depth-2;
      z=1.0f;
      }

   ptr1=&volume[(unsigned int)i+((unsigned int)j+(unsigned int)k*height)*width];
   ptr2=ptr1+width*height;

   return((1.0f-z)*((1.0f-y)*((1.0f-x)*ptr1[0]+x*ptr1[1])+
                    y*((1.0f-x)*ptr1[width]+x*ptr1[width+1]))+
          z*((1.0f-y)*((1.0f-x)*ptr2[0]+x*ptr2[1])+
             y*((1.0f-x)*ptr2[width]+x*ptr2[width+1])));
   }

// scale volume
unsigned char *mipmap::scale(unsigned char *volume,
                             unsigned int width,unsigned int height,unsigned int depth,
                             unsigned int nwidth,unsigned int nheight,unsigned int ndepth)
   {
   unsigned int i,j,k;

   unsigned char *volume2;

   if (nwidth==width && nheight==height && ndepth==depth) return(volume);

   if ((volume2=(unsigned char *)malloc(nwidth*nheight*ndepth))==NULL) ERRORMSG();

   for (i=0; i<nwidth; i++)
      for (j=0; j<nheight; j++)
         for (k=0; k<ndepth; k++)
            volume2[i+(j+k*nheight)*nwidth]=getscalar(volume,width,height,depth,(float)i/(nwidth-1),(float)j/(nheight-1),(float)k/(ndepth-1));

   free(volume);
   return(volume2);
   }

// read a volume by trying any known format
unsigned char *mipmap::readANYvolume(const char *filename,
                                     unsigned int *width,unsigned int *height,unsigned int *depth,unsigned int *components,
                                     float *scalex,float *scaley,float *scalez)
   {
   if (strchr(filename,'*')!=NULL)
      // read a DICOM series identified by the * in the filename pattern
      return(readDICOMvolume(filename,width,height,depth,components,scalex,scaley,scalez));
   else
      // read a PVM volume
      return(readPVMvolume(filename,width,height,depth,components,scalex,scaley,scalez));
   }

// read a DICOM series identified by the * in the filename pattern
unsigned char *mipmap::readDICOMvolume(const char *filename,
                                       unsigned int *width,unsigned int *height,unsigned int *depth,unsigned int *components,
                                       float *scalex,float *scaley,float *scalez)
   {
   DicomVolume data;
   unsigned char *chunk;

   if (!data.loadImages(filename)) return(NULL);

   if ((chunk=(unsigned char *)malloc(data.getVoxelNum()))==NULL) ERRORMSG();
   memcpy(chunk,data.getVoxelData(),data.getVoxelNum());

   *width=data.getCols();
   *height=data.getRows();
   *depth=data.getSlis();

   *components=1;

   if (scalex!=NULL) *scalex=data.getBound(0)/data.getCols();
   if (scaley!=NULL) *scaley=data.getBound(1)/data.getRows();
   if (scalez!=NULL) *scalez=data.getBound(2)/data.getSlis();

   return(chunk);
   }

// read a DICOM series from a file name list
unsigned char *mipmap::readDICOMvolume(const std::vector<std::string> list,
                                       unsigned int *width,unsigned int *height,unsigned int *depth,unsigned int *components,
                                       float *scalex,float *scaley,float *scalez)
   {
   DicomVolume data;
   unsigned char *chunk;

   if (!data.loadImages(list)) return(NULL);

   if ((chunk=(unsigned char *)malloc(data.getVoxelNum()))==NULL) ERRORMSG();
   memcpy(chunk,data.getVoxelData(),data.getVoxelNum());

   *width=data.getCols();
   *height=data.getRows();
   *depth=data.getSlis();

   *components=1;

   if (scalex!=NULL) *scalex=data.getBound(0)/data.getCols();
   if (scaley!=NULL) *scaley=data.getBound(1)/data.getRows();
   if (scalez!=NULL) *scalez=data.getBound(2)/data.getSlis();

   return(chunk);
   }

// load the volume and convert it to 8 bit
BOOLINT mipmap::loadvolume(const char *filename, // filename of PVM to load
                           const char *gradname, // optional filename of gradient volume
                           float mx,float my,float mz, // midpoint of volume (assumed to be fixed)
                           float sx,float sy,float sz, // size of volume (assumed to be fixed)
                           int bricksize,float overmax, // bricksize/overlap of volume (assumed to be fixed)
                           BOOLINT xswap,BOOLINT yswap,BOOLINT zswap, // swap volume flags
                           BOOLINT xrotate,BOOLINT zrotate, // rotate volume flags
                           BOOLINT usegrad, // use gradient volume
                           char *commands, // filter commands
                           int histmin,float histfreq,int kneigh,float histstep) // parameters for histogram computation
   {
   BOOLINT upload=FALSE;

   float maxsize;

   if (gradname==NULL) gradname=zerostr;
   if (commands==NULL) commands=zerostr;

   if (VOLUME==NULL ||
       strncmp(filename,filestr,MAXSTR)!=0 ||
       (usegrad && strlen(gradname)==0 && (GRAD==NULL || strlen(gradstr)>0)) ||
       strncmp(commands,commstr,MAXSTR)!=0 ||
       xswap!=xsflag || yswap!=ysflag || zswap!=zsflag ||
       xrotate!=xrflag || zrotate!=zrflag)
      {
      if (VOLUME!=NULL) free(VOLUME);
      if ((VOLUME=readANYvolume(filename,&WIDTH,&HEIGHT,&DEPTH,&COMPONENTS,&DSX,&DSY,&DSZ))==NULL) return(FALSE);

      if (COMPONENTS==2) VOLUME=quantize(VOLUME,WIDTH,HEIGHT,DEPTH);
      else if (COMPONENTS!=1)
         {
         free(VOLUME);
         return(FALSE);
         }

      VOLUME=swap(VOLUME,
                  &WIDTH,&HEIGHT,&DEPTH,
                  &DSX,&DSY,&DSZ,
                  xswap,yswap,zswap,
                  xrotate,zrotate);

      parsecommands(VOLUME,
                    WIDTH,HEIGHT,DEPTH,
                    commands);

      if (GRAD!=NULL)
         {
         free(GRAD);
         GRAD=NULL;
         }

      if (usegrad && strlen(gradname)==0)
         {
#ifdef MULTILEVEL
         GRAD=gradmagML(VOLUME,
                        WIDTH,HEIGHT,DEPTH,
                        DSX,DSY,DSZ,
                        &GRADMAX);
#else
         GRAD=gradmag(VOLUME,
                      WIDTH,HEIGHT,DEPTH,
                      DSX,DSY,DSZ,
                      &GRADMAX);
#endif

         parsegradcommands(VOLUME,GRAD,
                           WIDTH,HEIGHT,DEPTH,
                           commands);
         }

      strncpy(filestr,filename,MAXSTR);
      strncpy(gradstr,"",MAXSTR);
      strncpy(commstr,commands,MAXSTR);

      xsflag=xswap;
      ysflag=yswap;
      zsflag=zswap;

      xrflag=xrotate;
      zrflag=zrotate;

      upload=TRUE;
      }

   if (usegrad && strlen(gradname)>0)
      if (GRAD==NULL ||
          strncmp(gradname,gradstr,MAXSTR)!=0)
         {
         if (GRAD!=NULL) free(GRAD);
         if ((GRAD=readANYvolume(gradname,&GWIDTH,&GHEIGHT,&GDEPTH,&GCOMPONENTS))==NULL) exit(1);
         GRADMAX=1.0f;

         if (GCOMPONENTS==2) GRAD=quantize(GRAD,GWIDTH,GHEIGHT,GDEPTH);
         else if (GCOMPONENTS!=1) exit(1);

         GRAD=swap(GRAD,
                   &GWIDTH,&GHEIGHT,&GDEPTH,
                   NULL,NULL,NULL,
                   xswap,yswap,zswap,
                   xrotate,zrotate);

         GRAD=scale(GRAD,
                    GWIDTH,GHEIGHT,GDEPTH,
                    WIDTH,HEIGHT,DEPTH);

         parsegradcommands(VOLUME,GRAD,
                           WIDTH,HEIGHT,DEPTH,
                           commands);

         strncpy(gradstr,gradname,MAXSTR);

         upload=TRUE;
         }

   if (upload)
      {
      maxsize=fmax(DSX*(WIDTH-1),fmax(DSY*(HEIGHT-1),DSZ*(DEPTH-1)));

      set_data(VOLUME,
               usegrad?GRAD:NULL,
               WIDTH,HEIGHT,DEPTH,
               mx,my,mz,
               sx*DSX*(WIDTH-1)/maxsize,sy*DSY*(HEIGHT-1)/maxsize,sz*DSZ*(DEPTH-1)/maxsize,
               bricksize,overmax);
      }

   if (upload)
      HISTO->set_histograms(VOLUME,GRAD,WIDTH,HEIGHT,DEPTH,histmin,histfreq,kneigh,histstep);

   if (!upload && (hmvalue!=histmin || hfvalue!=histfreq))
      HISTO->inithist(VOLUME,WIDTH,HEIGHT,DEPTH,histmin,histfreq,FALSE);

   if (!upload && (hmvalue!=histmin || hfvalue!=histfreq || kneigh!=knvalue || histstep!=hsvalue))
      HISTO->inithist2DQ(VOLUME,GRAD,WIDTH,HEIGHT,DEPTH,histmin,histfreq,kneigh,histstep,FALSE);

   hmvalue=histmin;
   hfvalue=histfreq;
   knvalue=kneigh;
   hsvalue=histstep;

   return(TRUE);
   }

// load a DICOM series
BOOLINT mipmap::loadseries(const std::vector<std::string> list, // DICOM series to load
                           float mx,float my,float mz, // midpoint of volume (assumed to be fixed)
                           float sx,float sy,float sz, // size of volume (assumed to be fixed)
                           int bricksize,float overmax, // bricksize/overlap of volume (assumed to be fixed)
                           BOOLINT xswap,BOOLINT yswap,BOOLINT zswap, // swap volume flags
                           BOOLINT xrotate,BOOLINT zrotate, // rotate volume flags
                           int histmin,float histfreq,int kneigh,float histstep) // parameters for histogram computation
   {
   float maxsize;

   if (VOLUME!=NULL) free(VOLUME);
   if ((VOLUME=readDICOMvolume(list,&WIDTH,&HEIGHT,&DEPTH,&COMPONENTS,&DSX,&DSY,&DSZ))==NULL) return(FALSE);

   if (COMPONENTS==2) VOLUME=quantize(VOLUME,WIDTH,HEIGHT,DEPTH);
   else if (COMPONENTS!=1)
      {
      free(VOLUME);
      return(FALSE);
      }

   VOLUME=swap(VOLUME,
               &WIDTH,&HEIGHT,&DEPTH,
               &DSX,&DSY,&DSZ,
               xswap,yswap,zswap,
               xrotate,zrotate);

   strncpy(filestr,"",MAXSTR);
   strncpy(gradstr,"",MAXSTR);
   strncpy(commstr,"",MAXSTR);

   xsflag=xswap;
   ysflag=yswap;
   zsflag=zswap;

   xrflag=xrotate;
   zrflag=zrotate;

   maxsize=fmax(DSX*(WIDTH-1),fmax(DSY*(HEIGHT-1),DSZ*(DEPTH-1)));

   set_data(VOLUME,NULL,
            WIDTH,HEIGHT,DEPTH,
            mx,my,mz,
            sx*DSX*(WIDTH-1)/maxsize,sy*DSY*(HEIGHT-1)/maxsize,sz*DSZ*(DEPTH-1)/maxsize,
            bricksize,overmax);

   HISTO->set_histograms(VOLUME,NULL,WIDTH,HEIGHT,DEPTH,histmin,histfreq,kneigh,histstep);

   hmvalue=histmin;
   hfvalue=histfreq;
   knvalue=kneigh;
   hsvalue=histstep;

   return(TRUE);
   }

// save the volume data as PVM
void mipmap::savePVMvolume(const char *filename)
   {
   if (VOLUME==NULL) return;

   writePVMvolume(filename,VOLUME,
                  WIDTH,HEIGHT,DEPTH,COMPONENTS,
                  DSX,DSY,DSZ);
   }

// return the histogram
float *mipmap::get_hist()
   {
   if (VOLCNT==0) return(NULL);
   return(HISTO->get_hist());
   }

// return the colored histogram
float *mipmap::get_histRGBA()
   {
   if (VOLCNT==0) return(NULL);
   return(HISTO->get_histRGBA());
   }

// return the scatter plot
float *mipmap::get_hist2D()
   {
   if (VOLCNT==0) return(NULL);
   return(HISTO->get_hist2D());
   }

// return the colored scatter plot
float *mipmap::get_hist2DRGBA()
   {
   if (VOLCNT==0) return(NULL);
   return(HISTO->get_hist2DRGBA());
   }

// return the quantized scatter plot
float *mipmap::get_hist2DQRGBA()
   {
   if (VOLCNT==0) return(NULL);
   return(HISTO->get_hist2DQRGBA());
   }

// return the quantized transfer function
float *mipmap::get_hist2DTFRGBA()
   {
   if (VOLCNT==0) return(NULL);
   return(HISTO->get_hist2DTFRGBA());
   }

// check whether or not the hierarchy has volume data
BOOLINT mipmap::has_data()
   {return(VOLCNT!=0);}

// return the slab thickness
float mipmap::get_slab()
   {
   if (VOLCNT==0) return(0.0f);
   else return(VOL[0]->get_slab());
   }

// set ambient/diffuse/specular lighting coefficients
void mipmap::set_light(float noise,float ambnt,float difus,float specl,float specx)
   {
   int i;

   if (VOLCNT==0) return;

   for (i=0; i<VOLCNT; i++) VOL[i]->set_light(noise,ambnt,difus,specl,specx);
   }

// render the volume
void mipmap::render(float ex,float ey,float ez,
                    float dx,float dy,float dz,
                    float ux,float uy,float uz,
                    float nearp,float slab,
                    BOOLINT lighting)
   {
   int map=0;

   if (VOLCNT==0) return;

   if (TFUNC->get_imode())
      while (map<VOLCNT-1 && slab/VOL[map]->get_slab()>1.5f) map++;

   VOL[map]->render(ex,ey,ez,
                    dx,dy,dz,
                    ux,uy,uz,
                    nearp,slab,
                    1.0f/get_slab(),
                    lighting);

   if (TFUNC->get_invmode()) invertbuffer();
   }

// draw the surrounding wire frame box
void mipmap::drawwireframe()
   {
   if (VOLCNT==0) volume::drawwireframe();
   else
      volume::drawwireframe(VOL[0]->getcenterx(),VOL[0]->getcentery(),VOL[0]->getcenterz(),
                            VOL[0]->getsizex(),VOL[0]->getsizey(),VOL[0]->getsizez());
   }
