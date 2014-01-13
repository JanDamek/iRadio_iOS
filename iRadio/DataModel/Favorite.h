//
//  Favorite.h
//  iRadio
//
//  Created by Jan Damek on 12.01.14.
//  Copyright (c) 2014 Jan Damek. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <CoreData/CoreData.h>


@interface Favorite : NSManagedObject

@property (nonatomic, retain) NSDate * timeStamp;
@property (nonatomic, retain) NSManagedObject *radios_rel;

@end
