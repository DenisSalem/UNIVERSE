#include <iostream>
#include "common.hpp"
#include "realm.hpp"
#include <cmath>

Realm::Realm(int LoD, float * min, float * max) {
  this->LoD = LoD;
  this->min = min;
  this->max= max;
  this->scale = 2 << LoD;
  this->area = 2 << (LoD+LoD+1);
  this->stampCount = 0;
  this->neighbourTop = 0;
  this->neighbourBottom = 0;
  this->neighbourLeft = 0;
  this->neighbourRight = 0;
  this->realm = new float ** [MAXIMUM_NUMBER_OF_LAYERS];
  this->realm[0] = new float*[1]();

  for (int i = 0; i < MAXIMUM_NUMBER_OF_LAYERS-1; i++) {
    this->realm[i+1] = new float*[ 2 << (i * 2 + 1) ]();
  }

  this->AllocateChunk(0,0,0); 
}

inline void Realm::UpdateMinMax(int chunkIndex, float * chunk) {
  if (*this->max < chunk[ chunkIndex ]) {
    *this->max = chunk[ chunkIndex ];
  }
  if (*this->min <= 0 && *this->min > chunk[ chunkIndex ]) {
    *this->min = chunk[ chunkIndex ];
  }
}

void Realm::SetNeighbours(
      float *** neighbourTop,
      float *** neighbourBottom,
      float *** neighbourLeft,
      float *** neighbourRight
  ) {
  this->neighbourTop = neighbourTop;
  this->neighbourBottom = neighbourBottom;
  this->neighbourLeft = neighbourLeft;
  this->neighbourRight = neighbourRight;
};

void Realm::AddStamp(float * stamp) {
  this->stamps[this->stampCount]  = stamp;
  this->stampCount++;
}

void Realm::AllocateChunk(int layer, int chunkCoordX, int chunkCoordY) {
  this->realm[layer][chunkCoordX + chunkCoordY * (1 << layer)] = new float[this->area];
}

void Realm::DeallocateChunk(int layer, int chunkCoordX, int chunkCoordY) {
  delete [] this->realm[layer][chunkCoordX + chunkCoordY * (1 << layer)];
}

float * Realm::GetRealm(int layer, int chunkCoordX, int chunkCoordY) {
  return this->realm[layer][chunkCoordX + chunkCoordY * (1 << layer)];
}

float *** Realm::GetRealm() {
  return this->realm;
}

void Realm::Noise(int layer, int chunkCoordX, int chunkCoordY) {
  this->localChunk = this->realm[layer][chunkCoordX+chunkCoordY*(1<<layer)];
  this->Noise(layer, chunkCoordX, chunkCoordY, this->scale, 0, 0);
}

inline bool Realm::IsChunkAllocated(float *** dest, int layer, int chunkIndex) {
  if(dest != 0) {
    if (dest[layer] != 0) {
      if(dest[layer][ chunkIndex ] != 0) {
        return true;
      }
    }
  }
  return false;
}

inline bool Realm::IsChunkACorner(int x, int y, int limit) {
  if (x == 0 && y == 0) {
    return true;
  }
  if (x == limit && y == 0) {
    return true;
  }
  if (x == limit && y == limit) {
    return true;
  }
  if (x == 0 && y == limit) {
    return true;
  }
  return false;
}

inline bool Realm::DoesStampMatrixCrossCorner(int offsetX, int offsetY, int sectorScale) {
  bool a = offsetX < 0;
  bool b = offsetX+sectorScale >= this->scale;
  bool c = offsetY < 0;
  bool d = offsetY+sectorScale >= this->scale;
  if (a || b || c || d) {
    return true;
  }
  return false;
}

inline bool Realm::DoesStampCrossCorner(int offsetX, int offsetY, int sectorScale) {
  int stampRadius = sectorScale >> 1;
  int xCornerTopLeft = offsetX+stampRadius;
  int yCornerTopLeft = offsetY+stampRadius;
  int xCornerTopRight = stampRadius - (offsetX + sectorScale - this->scale);
  int yCornerTopRight = offsetY+stampRadius;
  int xCornerBottomRight = stampRadius - (offsetX + sectorScale - this->scale);
  int yCornerBottomRight = this->scale - (offsetY + stampRadius);
  int xCornerBottomLeft = offsetX+stampRadius;
  int yCornerBottomLeft = this->scale - (offsetY + stampRadius);

  // Coin gauche supérieur
  if(offsetX < 0 && offsetY < 0 && ceil(sqrt(pow(xCornerTopLeft,2) + pow(yCornerTopLeft,2))) <= stampRadius) {
      return true;
  }
  // Coin supérieur droit
  else if (offsetX+sectorScale >= this->scale && offsetY < 0 && ceil(sqrt(pow(xCornerTopRight,2) + pow(yCornerTopRight,2))) <= stampRadius) {
    return true;
  }
  // Coin inférieur droit
  else if (offsetX+sectorScale >= this->scale && offsetY+sectorScale >= this->scale && ceil(sqrt(pow(xCornerBottomRight,2) + pow(yCornerBottomRight,2))) <= stampRadius) {
    return true;
  }
  // Coin inférieur gauche
  else if (offsetX < 0 && offsetY+sectorScale >= this->scale && ceil(sqrt(pow(xCornerBottomLeft,2) + pow(yCornerBottomLeft,2))) <= stampRadius) {
    return true;
  }
  return false;
}

inline void Realm::StampWithinChunk(int x, int y, int stampIndex) {
  this->chunkIndex = y * this->scale + x;
  this->localChunk[ this->chunkIndex ] += this->sign * this->stamp[ stampIndex ] / this->inc;
  this->UpdateMinMax(this->chunkIndex, this->localChunk);
}

inline void Realm::StampBeyondBorder(int x, int y, int chunkCoordX, int chunkCoordY, int stampIndex) {
  // Le pixel courant est à gauche du secteur courant.
  if(this->horizontalNeighbourChunk != 0 && x < 0 && y >= 0 && y < this->scale) {
    // Le pixel courant se trouve sur la gauche du secteur courant, mais à l'extérieur du royaume.
    if (chunkCoordX == 0) {
      this->chunkIndex = this->getCoordsToNeighbourLeft(x, y, this->scale) ;
    }
    // Le pixel courant se trouve sur la gauche du secteur courant, mais à l'intérieur du royaume.
    else {
      this->chunkIndex = y * this->scale + this->scale + x;
    }
    horizontalNeighbourChunk[chunkIndex] += this->sign * this->stamp[ stampIndex ] / this->inc;
    this->UpdateMinMax(chunkIndex, this->horizontalNeighbourChunk);
  }
  // Le pixel courant est à droite du secteur courant.
  else if (this->horizontalNeighbourChunk != 0 && x >= this->scale && y >= 0 && y < this->scale) {
    // Le pixel courant se trouve sur la droite du secteur courant, mais à l'extérieur du royaume.
    if (chunkCoordX == this->chunkScale - 1) {
      this->chunkIndex = this->getCoordsToNeighbourRight(x, y, this->scale) ;
    }
    // Le pixel courant se trouve sur la droite du secteur courant, mais à l'intérieur du royaume.
    else {
      this->chunkIndex = (y) * this->scale + x - this->scale;
    }
    horizontalNeighbourChunk[chunkIndex] += this->sign * this->stamp[ stampIndex ] / this->inc;
    this->UpdateMinMax(chunkIndex, this->horizontalNeighbourChunk);
  }
  // Le pixel courant est en bas du secteur courant
  else if (this->verticalNeighbourChunk != 0 && y >= this->scale && x >= 0 && x < this->scale) {
  // Le pixel courant se trouve en bas du secteur courant, mais à l'extérieur du royaume.
    if (chunkCoordY == this->chunkScale - 1) {
      this->chunkIndex = this->getCoordsToNeighbourBottom( x, y, this->scale) ;
    }
    // Le pixel courant se trouve en bas du secteur courant, mais à l'intérieur du royaume.
    else {
      this->chunkIndex = (y-this->scale) * this->scale + x;
    }
    this->verticalNeighbourChunk[ chunkIndex ] += this->sign * this->stamp[ stampIndex ] / this->inc;
    this->UpdateMinMax(chunkIndex, this->verticalNeighbourChunk);
  }
  // Le pixel courant est en haut du secteur courant
  else if (this->verticalNeighbourChunk != 0 && y < 0 && x >= 0 && x < this->scale) {
    // Le pixel courant se trouve en haut du secteur courant, mais à l'extérieur du royaume.
    if (chunkCoordY == 0) {
      this->chunkIndex = this->getCoordsToNeighbourTop(x, y, this->scale) ;
    }
    // Le pixel courant se trouve en haut du secteur courant, mais à l'intérieur du royaume.
    else {
      this->chunkIndex = (this->scale + y) * this->scale + x;
    }
    verticalNeighbourChunk[ chunkIndex ] += this->sign * this->stamp[ stampIndex ] / this->inc;
    this->UpdateMinMax(chunkIndex, this->verticalNeighbourChunk);
  }
}

// Selon le cas de figure, le dépassement du tampon peut déborder sur au plus trois chunk:
// Pour éviter de déréférencer 10 milles fois, on prépare les destinations en fonctions des paramétres
// de la récursion courante.
//
// On se propose donc deux fonctions pour préparer tout ça.

inline void Realm::PrepareDestinationOnCorner(int layer, int chunkCoordX, int chunkCoordY, int offsetX, int offsetY, int sectorScale) {
  // Dépassement sur le coin supérieur gauche
  if (offsetX < 0 && offsetY < 0) {
    // Une seule coordonné à tester suffit parce qu'on sait qu'on dépasse sur un coin dans tous les cas.
    if(chunkCoordX == 0) {
    }
    else {
      this->diagonalNeighbourChunk = this->realm[layer][chunkCoordX - 1 + (chunkCoordY-1) * chunkScale];
    }
  }
  // Dépassement sur le coin supérieur droit
  else if (offsetX+sectorScale >= this->scale && offsetY < 0) {
    if(chunkCoordX == chunkScale - 1) {
    }
    else {
      this->diagonalNeighbourChunk = this->realm[layer][chunkCoordX + 1 + (chunkCoordY - 1) * chunkScale];
    }
  }
  // Dépassement sur le coin inférieur droit
  else if (offsetX+sectorScale >= this->scale && offsetY+sectorScale >= this->scale) {
    if(chunkCoordX == chunkScale - 1) {
    }
    else {
      this->diagonalNeighbourChunk = this->realm[layer][chunkCoordX + 1 + (chunkCoordY + 1) * chunkScale];
    }
  }
  // Dépassement sur le coin inférieur gauche
  else if (offsetX < 0 && this->scale && offsetY+sectorScale >= this->scale) {
    if(chunkCoordX == chunkScale - 1) {
    }
    else {
      this->diagonalNeighbourChunk = this->realm[layer][chunkCoordX - 1 + (chunkCoordY + 1) * chunkScale];
    }
  }
}

inline void Realm::PrepareDestinationOnBorder(int layer, int chunkCoordX, int chunkCoordY, int offsetX, int offsetY, int sectorScale) {

  this->horizontalNeighbourChunkCoord = 0;
  this->verticalNeighbourChunkCoord = 0;

  // Dépassement sur la gauche
  if (offsetX < 0) {
    if (chunkCoordX == 0) {
      this->horizontalNeighbourChunkCoord = this->getCoordsToNeighbourLeft(-1, chunkCoordY, chunkScale);
      if (this->IsChunkAllocated(this->neighbourLeft, layer, horizontalNeighbourChunkCoord) == true) {
        this->horizontalNeighbourChunk = this->neighbourLeft[layer][horizontalNeighbourChunkCoord ]; 
      }
    }
    else {
      this->horizontalNeighbourChunkCoord = chunkCoordX - 1 + chunkCoordY * chunkScale;
      this->horizontalNeighbourChunk = this->realm[layer][horizontalNeighbourChunkCoord];
    }
  }

  //Dépassement sur la droite
  else if (offsetX+sectorScale >= this->scale) {
    if (chunkCoordX == chunkScale - 1) {
      this->horizontalNeighbourChunkCoord = this->getCoordsToNeighbourRight(chunkCoordX+1, chunkCoordY, chunkScale);
      if (this->IsChunkAllocated(this->neighbourRight, layer, horizontalNeighbourChunkCoord) == true) {
        this->horizontalNeighbourChunk = this->neighbourRight[layer][horizontalNeighbourChunkCoord ]; 
      }
    }
    else {
      this->horizontalNeighbourChunkCoord = chunkCoordX + 1 + chunkCoordY * chunkScale;
      this->horizontalNeighbourChunk = this->realm[layer][horizontalNeighbourChunkCoord];
    }
  }
  //Dépassement vers le bas
  if (offsetY+sectorScale >= this->scale) {
    if (chunkCoordY == chunkScale - 1) {
      this->verticalNeighbourChunkCoord = this->getCoordsToNeighbourBottom(chunkCoordX, chunkCoordY+1, chunkScale);
      if (this->IsChunkAllocated(this->neighbourBottom, layer, verticalNeighbourChunkCoord) == true) {
        this->verticalNeighbourChunk = this->neighbourBottom[layer][verticalNeighbourChunkCoord ]; 
      }
    }
    else {
      this->verticalNeighbourChunkCoord = chunkCoordX + (chunkCoordY+1) * chunkScale;
      this->verticalNeighbourChunk = this->realm[layer][verticalNeighbourChunkCoord];
    }
  }
  //Dépassement vers le haut
  else if (offsetY < 0 ) {
    if (chunkCoordY == 0) {
      verticalNeighbourChunkCoord = this->getCoordsToNeighbourTop(chunkCoordX, -1, chunkScale);
      if (this->IsChunkAllocated(this->neighbourTop, layer, verticalNeighbourChunkCoord) == true) {
        this->verticalNeighbourChunk = this->neighbourTop[layer][ verticalNeighbourChunkCoord ]; 
      }
    }
    else {
      verticalNeighbourChunkCoord = chunkCoordX + (chunkCoordY-1) * chunkScale;
      this->verticalNeighbourChunk = this->realm[layer][verticalNeighbourChunkCoord];
    }
  }
}

void Realm::Noise(int layer, int chunkCoordX, int chunkCoordY, int sectorScale, int sectorStartU, int sectorStartV) {
  if (sectorScale == 1) {
    return;
  }

  this->sign = getRandom() & 1 ? 1.0 : -1.0;
  this->inc = this->scale / sectorScale;
  this->horizontalNeighbourChunk=0;
  this->verticalNeighbourChunk=0;
  this->diagonalNeighbourChunk=0;

  // Cette curieuse formule permet de connaitre pour un calque donné la dimension
  // d'un morceau courant. Habile!
  this->chunkScale = 1 << layer;

  int halfScale = sectorScale >> 1;
  int randX = - halfScale + getRandom() % (sectorScale);
  int randY = - halfScale + getRandom() % (sectorScale);
  int stampId =  getRandom() % this->stampCount;
  int stampIndex;
  int offsetX = -randX + sectorStartU;
  int offsetY = -randY + sectorStartV;
  int stampX=0,stampY=0;
  double distanceFromStampCenterToCorner;
  bool stampMatrixCrossCorner = this->DoesStampMatrixCrossCorner(offsetX,offsetY,sectorScale);
  bool stampCrossCorner = this->DoesStampCrossCorner(offsetX,offsetY,sectorScale);
  int localChunkCoord = chunkCoordX + chunkCoordY * chunkScale;
  this->stamp = this->stamps[stampId];
  this->PrepareDestinationOnCorner(layer, chunkCoordX, chunkCoordY,offsetX,offsetY,sectorScale);
  this->PrepareDestinationOnBorder(layer, chunkCoordX, chunkCoordY,offsetX,offsetY,sectorScale);

  if ( this->IsChunkACorner(chunkCoordX,chunkCoordY,chunkScale-1) && stampCrossCorner) {
  }
  else {
    for (int y=0; y<sectorScale; y++) {
      stampX = 0;
      for (int x=0; x<sectorScale; x++) {
        stampIndex = stampX + sectorScale * stampY;
	if ( stamp[ stampIndex ] != 0) {
          if (x+offsetX >= 0 && x+offsetX < this->scale && y+offsetY >= 0 && y+offsetY < this->scale) {
            this->StampWithinChunk(x+offsetX, y+offsetY, stampIndex);
          }
          else if(stampCrossCorner) {
          }
          else {
            StampBeyondBorder(x+offsetX, y+offsetY, chunkCoordX, chunkCoordY, stampIndex);
          }
        }
        stampX += inc;
      }
      stampY += inc * inc;
    }
  }
  
  this->Noise(layer, chunkCoordX, chunkCoordY, halfScale, sectorStartU,		  sectorStartV);
  this->Noise(layer, chunkCoordX, chunkCoordY, halfScale, sectorStartU, 	  sectorStartV+halfScale);
  this->Noise(layer, chunkCoordX, chunkCoordY, halfScale, sectorStartU+halfScale, sectorStartV+halfScale);
  this->Noise(layer, chunkCoordX, chunkCoordY, halfScale, sectorStartU+halfScale, sectorStartV);
};
